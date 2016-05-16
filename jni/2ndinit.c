/*
 * Copyright (c) 2016 rbox
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <fcntl.h>
#include <dirent.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/ptrace.h>
#include <sys/sendfile.h>
#include <sys/wait.h>

#include "archive.h"
#include "archive_entry.h"
#include "sepol/policydb/services.h"

#define RECOVERY_RAMDISK "ramdisk-recovery.cpio.lzma"
#define SYSTEM_RECOVERY_RAMDISK "/system/recovery/" RECOVERY_RAMDISK
#define MNT_DIR "/mnt"
#define MNT_RECOVERY_RAMDISK MNT_DIR "/" RECOVERY_RAMDISK

static void set_permissive(const hashtab_key_t domain, policydb_t *pdb)
{
    type_datum_t *type = hashtab_search(pdb->p_types.table, domain);
    ebitmap_set_bit(&pdb->permissive_map, type->s.value, 1);
}

static void selinux_permissive(void)
{
    policydb_t policydb;
    struct policy_file pf;

    policydb_init(&policydb);
    sepol_set_policydb(&policydb);
    policy_file_init(&pf);

    // Read the current policy
    pf.fp = fopen("/sepolicy", "r");
    pf.type = PF_USE_STDIO;
    policydb_read(&policydb, &pf, 0);
    fclose(pf.fp);

    // Make init, recovery, and ueventd permissive
    set_permissive("init", &policydb);
    set_permissive("recovery", &policydb);
    set_permissive("ueventd", &policydb);

    // Write the new policy and load it
    pf.fp = fopen("/dev/sepolicy", "w+");
    policydb_write(&policydb, &pf);
    int size = ftell(pf.fp);
    fseek(pf.fp, SEEK_SET, 0);
    void *map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fileno(pf.fp), 0);
    int load = open("/sys/fs/selinux/load", O_WRONLY);
    write(load, map, size);
    close(load);
    munmap(map, size);
    fclose(pf.fp);

    policydb_destroy(&policydb);
}

static void extract_recovery(char *ramdisk_path)
{
    // Delete everything in the current root
    DIR *root = opendir("/");
    struct dirent *dp;
    while ((dp = readdir(root)) != NULL)
    {
        unlink(dp->d_name);
    }
    closedir(root);

    // Open the ramdisk
    struct archive *a = archive_read_new();
    archive_read_support_filter_lzma(a);
    archive_read_support_format_cpio(a);
    if (archive_read_open_filename(a, ramdisk_path, 4096) == ARCHIVE_OK)
    {
        // Set up the writer
        struct archive *ext = archive_write_disk_new();
        archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_UNLINK | ARCHIVE_EXTRACT_PERM);
        chdir("/");

        while (1)
        {
            struct archive_entry *entry;

            // Read the next file
            if (archive_read_next_header(a, &entry) == ARCHIVE_EOF)
                break;

            // Create the file
            archive_write_header(ext, entry);

            // If the file has data to write...
            if (!archive_entry_size_is_set(entry) || archive_entry_size(entry) > 0)
            {
                const void *block;
                size_t size;
                int64_t offset;

                // Write the blocks until EOF
                while (1)
                {
                    if (archive_read_data_block(a, &block, &size, &offset) == ARCHIVE_EOF)
                        break;
                    archive_write_data_block(ext, block, size, offset);
                }
            }
        }

        archive_write_free(ext);
    }

    archive_read_free(a);
}

static void read_init_map(char *wanted_dev, unsigned long *base)
{
    char line[128];

    FILE *fp = fopen("/proc/1/maps", "r");
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        if (strstr(line, wanted_dev))
        {
            sscanf(line, "%lx", base);
            break;
        }
    }
    fclose(fp);
}

static unsigned long find_execve(unsigned long image_base)
{
#ifdef __aarch64__
    /*===================================
     * execve:
     *
     * d2801ba8  mov   x8, #0xdd
     * d4000001  svc   #0x0
     * b140041f  cmn   x0, #0x1, lsl #12
     * da809400  cneg  x0, x0, hi
     *=================================*/
    const int execve_code[] = { 0xd2801ba8, 0xd4000001, 0xb140041f, 0xda809400 };
#else
    /*====================================
     * execve:
     *
     * e1a0c007        mov     ip, r7
     * e3a0700b        mov     r7, #11
     *==================================*/
    const int execve_code[] = { 0xe1a0c007, 0xe3a0700b };
#endif
    unsigned long i;

    for (i = 0; ; i += sizeof(*execve_code))
    {
        unsigned long buffer[2];

        // Read the next 8 or 16 bytes
        buffer[0] = ptrace(PTRACE_PEEKTEXT, 1, image_base + i, NULL);
        buffer[1] = ptrace(PTRACE_PEEKTEXT, 1, image_base + i + sizeof(*buffer), NULL);

        // compare them to the execve instructions
        if (memcmp(buffer, execve_code, sizeof(buffer)) == 0)
        {
            // Found the address of execve
            return image_base + i;
        }
    }

    // execve not found
    return 0;
}

static void replace_init(void)
{
    // Attach to existing init and wait for an interrupt
    ptrace(PTRACE_ATTACH, 1, NULL, NULL);
    wait(NULL);

    // Get address to inject to
    unsigned long data_inject_address = 0;
    read_init_map("00:00", &data_inject_address);

    // Get init text address
    unsigned long text_base = 0;
    read_init_map("00:01", &text_base);

    // Find the address of execve in the init text
    unsigned long execve_address = find_execve(text_base);

    /*
     * Inject the data
     *
     * argv[0]         - pointer to "/init"
     * argv[1]/envp[0] - NULL
     * "/init"
     */
    ptrace(PTRACE_POKEDATA, 1, data_inject_address + (sizeof(void *) * 0), data_inject_address + 0x10);
    ptrace(PTRACE_POKEDATA, 1, data_inject_address + (sizeof(void *) * 1), 0);
#ifdef __aarch64__
    ptrace(PTRACE_POKEDATA, 1, data_inject_address + (sizeof(void *) * 2), *(unsigned long *)"/init\0\0\0");
#else
    ptrace(PTRACE_POKEDATA, 1, data_inject_address + (sizeof(void *) * 2), *(unsigned long *)"/ini");
    ptrace(PTRACE_POKEDATA, 1, data_inject_address + (sizeof(void *) * 3), *(unsigned long *)"t\0\0\0");
#endif

    // Get inits current registers
#ifdef __aarch64__
    struct iovec ioVec;
    struct user_pt_regs regs[1];
    ioVec.iov_base = regs;
    ioVec.iov_len = sizeof(*regs);
    ptrace(PTRACE_GETREGSET, 1, NT_PRSTATUS, &ioVec);
#else
    struct pt_regs regs;
    ptrace(PTRACE_GETREGS, 1, NULL, &regs);
#endif

    // Change the registers to call execve("/init", argv, envp)
#ifdef __aarch64__
    regs->regs[0] = data_inject_address + 0x10; /* char *filename */
    regs->regs[1] = data_inject_address + 0x00; /* char *argv[] */
    regs->regs[2] = data_inject_address + 0x08; /* char *envp[] */
    regs->pc = execve_address;
    ptrace(PTRACE_SETREGSET, 1, NT_PRSTATUS, &ioVec);
#else
    regs.ARM_r0 = data_inject_address + (sizeof(void *) * 2); /* char*  filename */
    regs.ARM_r1 = data_inject_address + (sizeof(void *) * 0); /* char** argp */
    regs.ARM_r2 = data_inject_address + (sizeof(void *) * 1); /* char** envp */
    regs.ARM_pc = execve_address;
    ptrace(PTRACE_SETREGS, 1, NULL, &regs);
#endif

    // Detach the ptrace
    ptrace(PTRACE_DETACH, 1, NULL, NULL);
}

int main(int argc, char **argv)
{
    struct stat sStat;
    char *ramdisk_path = NULL;

    // Make sure this is being called from the 2ndinit stub
    if (argc < 2 || strcmp(argv[1], "2ndinit"))
    {
        return -1;
    }

    // Modify the selinux policy to make init permissive
    selinux_permissive();

    // sloane needs USB to be enabled
    FILE *mode = fopen("/sys/devices/bus.8/11270000.SSUSB/mode", "w");
    if (mode)
    {
        fwrite("1", 1, 1, mode);
        fclose(mode);
        sleep(3);
    }

    // Check for the ramdisk on secondary storage first
    char *devices[] = { "/dev/block/mmcblk1", "/dev/block/mmcblk1p1",
                        "/dev/block/sda", "/dev/block/sda1" };
    int i;
    for (i = 0; i < (sizeof(devices) / sizeof(char *)) && (ramdisk_path == NULL); i++)
    {
        mount(devices[i], MNT_DIR, "vfat", 0, NULL);
        if (stat(MNT_RECOVERY_RAMDISK, &sStat) == 0)
            ramdisk_path = MNT_RECOVERY_RAMDISK;
        else
            umount(MNT_DIR);
    }

    // If the ramdisk wasn't found, try /system
    if (ramdisk_path == NULL && stat(SYSTEM_RECOVERY_RAMDISK, &sStat) == 0)
        ramdisk_path = SYSTEM_RECOVERY_RAMDISK;

    // If the ramdisk was found
    if (ramdisk_path != NULL)
    {
        // Extract the ramdisk
        extract_recovery(ramdisk_path);

        // Unmount secondary storage if the ramdisk was on it
        if (!strcmp(ramdisk_path, MNT_RECOVERY_RAMDISK))
            umount(MNT_DIR);

        // stop adbd
        system("/system/bin/setprop ctl.stop adbd");

        // Use ptrace to replace init
        replace_init();

        return 0;
    }

    return -1;
}

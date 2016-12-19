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
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define BYPASS_2NDINIT "/cache/bypass_2ndinit"
#define DAEMONSU "/system/xbin/daemonsu"
#define PRIMARY_2NDINIT "/system/recovery/2ndinit"
#ifdef __arch64__
#define SECONDARY_2NDINIT "/system/bin/pppd"
#else
#define SECONDARY_2NDINIT "/dev/null"
#endif

int main(int argc, char **argv)
{
    int r;
    struct stat sStat;

    // bueller and montoya run 2ndinitstub as e2fsck
    if (!strcmp(argv[0], "/system/bin/e2fsck"))
    {
        // Override -fy /dev/block/platform/msm_sdcc.1/by-name/userdata on bueller
        if (argc == 3 && !strcmp(argv[1], "-fy") && !strcmp(argv[2], "/dev/block/platform/msm_sdcc.1/by-name/userdata"))
        {
            // continue below
        }
        // Override -y /dev/block/platform/sdhci.1/by-name/userdata on montoya
        else if (argc == 3 && !strcmp(argv[1], "-y") && !strcmp(argv[2], "/dev/block/platform/sdhci.1/by-name/userdata"))
        {
            // continue below
        }
        else
        {
            argv[0] = "/system/bin/e2fsck.real";
            return execv(argv[0], argv);
        }
    }

    /* check /cache for the bypass flag
     * it must be mounted on montoya and sloane. */
    r = mount("/dev/block/platform/mtk-msdc.0/by-name/cache", "/cache", "ext4", 0, NULL);
    if (r == -1)
    {
        /* montoya mount point */
        r = mount("/dev/block/platform/sdhci.1/by-name/cache", "/cache", "ext4", 0, NULL);
    }
    if (stat(BYPASS_2NDINIT, &sStat) == 0)
    {
        // If the bypass flag exists, delete the file and boot normally.
        unlink(BYPASS_2NDINIT);

        // Only unmount /cache if the mount didn't fail
        if (r == 0) umount("/cache");
    }
    else
    {
        if (fork() == 0)
        {
            // Look for primary 2ndinit, and if not found, use secondary 2ndinit
            char *exec = (stat(PRIMARY_2NDINIT, &sStat) == 0) ? PRIMARY_2NDINIT : SECONDARY_2NDINIT;
            return execl(exec, exec, "2ndinit", NULL);
        }
        else
        {
            int statval;
            wait(&statval);
            if (WIFEXITED(statval) && WEXITSTATUS(statval) == 0)
            {
                // if 2ndinit exited normally, end here
                return 0;
            }
        }
    }

    // Try to start daemonsu
    if (fork() == 0)
    {
        execl(DAEMONSU, DAEMONSU, "--auto-daemon", NULL);
    }

    return 0;
}

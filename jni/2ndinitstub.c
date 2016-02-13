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
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define BYPASS_2NDINIT "/cache/bypass_2ndinit"
#define DAEMONSU "/system/xbin/daemonsu"
#define PRIMARY_2NDINIT "/system/recovery/2ndinit"
#define SECONDARY_2NDINIT "/system/bin/pppd"

int main(int argc, char **argv)
{
    struct stat sStat;
    char *exec;

    // mount /cache to check for the bypass flag
    mount("/dev/block/platform/mtk-msdc.0/by-name/cache", "/cache", "ext4", 0, NULL);
    if (stat(BYPASS_2NDINIT, &sStat) == 0)
    {
        // If the bypass flag exists, delete the file and boot normally.
        unlink(BYPASS_2NDINIT);
        umount("/cache");

        // Start daemonsu if found
        if (stat(DAEMONSU, &sStat) == 0 && fork() == 0)
        {
            execl(DAEMONSU, DAEMONSU, "--auto-daemon", NULL);
            exit(1);
        }

        return 0;
    }
    umount("/cache");

    // Look for primary 2ndinit, and if not found, use secondary 2ndinit
    exec = (stat(PRIMARY_2NDINIT, &sStat) == 0) ? PRIMARY_2NDINIT : SECONDARY_2NDINIT;
    return execl(exec, exec, "2ndinit", NULL);
}

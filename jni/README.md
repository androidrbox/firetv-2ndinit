# Fire TV 2 2ndinit

This 2ndinit program allows the Fire TV 2 to load an alternative ramdisk on
boot, such as recovery.  It is based off of 2ndinit from Safestrap
(https://github.com/Hashcode/android_bootable_recovery/blob/twrp2.7-safestrap/safestrap/2nd-init/2nd-init.c)
and SELinux disabling code from Superuser
(https://github.com/phhusson/Superuser/tree/stableL/Superuser/jni/placeholder).

This git repository uses submodules and should be cloned with
git clone --recursive.

Compiling this requires the Android NDK.  2ndinitstub should be copied over
/system/bin/ext4_resize and 2ndinit should be copied to
/system/recovery/2ndinit (or /system/bin/pppd).

When the Fire TV 2 boots, after doing some initialization, it mounts the
/system partition.  Immediately after that, it runs /system/bin/ext4_resize.
By replacing that with 2ndinitstub, the stub will run immediately on boot.
The stub checks for a flag to determine if it should boot normally to Android
or replace the ramdisk.  If replacing the ramdisk, it will run 2ndinit.  First
it will check /system/recovery/2ndinit.  If that file does not exist, it will
run /system/bin/pppd.

When 2ndinit runs, first it will put SELinux into permissive mode to allow
2ndinit to do it's thing.  Then it will search for the recovery ramdisk.
First it looks in /system/recovery/ramdisk-recovery.cpio.  If that is not
found, it will enable USB and look for the file in the root of an attached USB
storage device.  If the ramdisk was found, It will delete the files in the
current ramdisk and extract the new one.  Then it will replace the kernel
cmdline file giving a flag to init so it will not try to reinitialize SELinux.
Finally, it will use ptrace to replace init.

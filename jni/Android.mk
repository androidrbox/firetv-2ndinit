 # Copyright (c) 2016 rbox
 #
 # This program is free software: you can redistribute it and/or modify it
 # under the terms of the GNU General Public License as published by the Free
 # Software Foundation, either version 3 of the License, or (at your option)
 # any later version.
 #
 # This program is distributed in the hope that it will be useful, but WITHOUT
 # ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 # FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 # more details.
 #
 # You should have received a copy of the GNU General Public License along
 # with this program.  If not, see <http://www.gnu.org/licenses/>.

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := 2ndinitstub
LOCAL_CFLAGS := -Wall -Os
LOCAL_LDFLAGS := -Wl,--build-id=0x$(shell git describe --abbrev=40 --always --tags)
LOCAL_SRC_FILES := 2ndinitstub.c
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := 2ndinit
LOCAL_CFLAGS := -Wall -Os -I. -I$(LOCAL_PATH)/libarchive/libarchive \
	-I$(LOCAL_PATH)/libsepol/include -I$(LOCAL_PATH)/libsepol/src \
	-I$(LOCAL_PATH)/lzma/src/liblzmadec -DHAVE_CONFIG_H
LOCAL_LDFLAGS := -Wl,--build-id=0x$(shell git describe --abbrev=40 --always --tags)
LOCAL_SRC_FILES := 2ndinit.c \
	libarchive/libarchive/archive_acl.c \
	libarchive/libarchive/archive_check_magic.c \
	libarchive/libarchive/archive_entry.c \
	libarchive/libarchive/archive_entry_sparse.c \
	libarchive/libarchive/archive_entry_xattr.c \
	libarchive/libarchive/archive_read.c \
	libarchive/libarchive/archive_read_open_filename.c \
	libarchive/libarchive/archive_read_support_filter_xz.c \
	libarchive/libarchive/archive_read_support_format_cpio.c \
	libarchive/libarchive/archive_string.c \
	libarchive/libarchive/archive_string_sprintf.c \
	libarchive/libarchive/archive_util.c \
	libarchive/libarchive/archive_virtual.c \
	libarchive/libarchive/archive_write.c \
	libarchive/libarchive/archive_write_disk_acl.c \
	libarchive/libarchive/archive_write_disk_posix.c \
	libsepol/src/avrule_block.c \
	libsepol/src/avtab.c \
	libsepol/src/conditional.c \
	libsepol/src/constraint.c \
	libsepol/src/context.c \
	libsepol/src/debug.c \
	libsepol/src/ebitmap.c \
	libsepol/src/expand.c \
	libsepol/src/hashtab.c \
	libsepol/src/mls.c \
	libsepol/src/policydb.c \
	libsepol/src/services.c \
	libsepol/src/sidtab.c \
	libsepol/src/symtab.c \
	libsepol/src/util.c \
	libsepol/src/write.c \
	lzma/src/liblzmadec/main.c
include $(BUILD_EXECUTABLE)

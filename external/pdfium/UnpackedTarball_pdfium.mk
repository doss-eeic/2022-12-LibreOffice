# -*- Mode: makefile-gmake; tab-width: 4; indent-tabs-mode: t -*-
#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

pdfium_patches :=
pdfium_patches += ubsan.patch
# Fixes build on our baseline.
pdfium_patches += build.patch.1
# Avoids Windows 8 build dependency.
pdfium_patches += windows7.patch.1
pdfium_patches += c++20-comparison.patch
# Use CoreGraphics.h instead of Carbon.h
pdfium_patches += cg-instead-of-carbon.patch.1
# Android NDK 19 - that is known to work well - does not have 2 defines
pdfium_patches += AndroidNDK19.patch.1

# Work around <https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94141> "c++20 rewritten operator==
# recursive call mixing friend and external operators for template class" in GCC with
# --with-latest-c++:
pdfium_patches += gcc-c++20-comparison.patch

pdfium_patches += include.patch

pdfium_patches += abseil-trivial.patch

# https://pdfium-review.googlesource.com/c/pdfium/+/97950
pdfium_patches += Add-FPDFAnnot_GetFormAdditionalActionJavaScript-API.patch.1
# https://pdfium-review.googlesource.com/c/pdfium/+/98210
pdfium_patches += Add-FPDFAnnot_GetFormFieldAlternateName-API.patch.1

$(eval $(call gb_UnpackedTarball_UnpackedTarball,pdfium))

$(eval $(call gb_UnpackedTarball_set_tarball,pdfium,$(PDFIUM_TARBALL)))

$(eval $(call gb_UnpackedTarball_set_patchlevel,pdfium,0))

$(eval $(call gb_UnpackedTarball_add_patches,pdfium,\
    $(foreach patch,$(pdfium_patches),external/pdfium/$(patch)) \
))

$(eval $(call gb_UnpackedTarball_set_post_action,pdfium,\
    mv third_party/bigint/BigInteger.cc third_party/bigint/BigInteger.cpp && \
    mv third_party/bigint/BigIntegerUtils.cc third_party/bigint/BigIntegerUtils.cpp && \
    mv third_party/bigint/BigUnsigned.cc third_party/bigint/BigUnsigned.cpp && \
    mv third_party/bigint/BigUnsignedInABase.cc third_party/bigint/BigUnsignedInABase.cpp && \
    mv third_party/base/allocator/partition_allocator/address_space_randomization.cc third_party/base/allocator/partition_allocator/address_space_randomization.cpp && \
    mv third_party/base/allocator/partition_allocator/page_allocator.cc third_party/base/allocator/partition_allocator/page_allocator.cpp && \
    mv third_party/base/allocator/partition_allocator/partition_alloc.cc third_party/base/allocator/partition_allocator/partition_alloc.cpp && \
    mv third_party/base/allocator/partition_allocator/spin_lock.cc third_party/base/allocator/partition_allocator/spin_lock.cpp && \
    mv third_party/base/debug/alias.cc third_party/base/debug/alias.cpp && \
    mv third_party/base/allocator/partition_allocator/oom_callback.cc third_party/base/allocator/partition_allocator/oom_callback.cpp && \
    mv third_party/base/allocator/partition_allocator/partition_bucket.cc third_party/base/allocator/partition_allocator/partition_bucket.cpp && \
    mv third_party/base/allocator/partition_allocator/partition_oom.cc third_party/base/allocator/partition_allocator/partition_oom.cpp && \
    mv third_party/base/allocator/partition_allocator/partition_page.cc third_party/base/allocator/partition_allocator/partition_page.cpp && \
    mv third_party/base/allocator/partition_allocator/partition_root_base.cc third_party/base/allocator/partition_allocator/partition_root_base.cpp && \
    mv third_party/base/allocator/partition_allocator/random.cc third_party/base/allocator/partition_allocator/random.cpp && \
    mv third_party/base/memory/aligned_memory.cc third_party/base/memory/aligned_memory.cpp && \
    mv third_party/base/win/win_util.cc third_party/base/win/win_util.cpp && \
    mv third_party/libopenjpeg/opj_malloc.cc third_party/libopenjpeg/opj_malloc.cpp && \
    mv third_party/abseil-cpp/absl/types/bad_optional_access.cc third_party/abseil-cpp/absl/types/bad_optional_access.cpp && \
    mv third_party/abseil-cpp/absl/types/bad_variant_access.cc third_party/abseil-cpp/absl/types/bad_variant_access.cpp \
))

# vim: set noet sw=4 ts=4:

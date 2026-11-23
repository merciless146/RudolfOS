# 1.6.2 (docs/plan-1.6.2.md Part 31, item 3): how the system's images are packed, in one place for every script that makes
# them (mklayers.sh, ci_release.sh, testkernel.sh, arm64_kernel.sh). zstd since 1.6.2 (xz -Xbcj x86 -b 1M before), at the
# level and block size the measurements chose (Part 31's table: .github/workflows/zstd-measure.yml). Every kernel since
# 1.6.2 reads it (SQUASHFS_ZSTD; the release list says so, and rudolf-update refuses images a release's kernel cannot read).
# Sourced (sh and bash). SQ_COMP_ID and SQ_BLOCK: what a layer's superblock then says (the compression's number at byte
# 20, the block size at byte 12), for ci_release.sh's check that every layer is packed so.
SQ_LEVEL=22
SQ_BLOCK=1048576
SQ_COMP_ID=6
SQ_OPTS="-comp zstd -Xcompression-level $SQ_LEVEL -b $SQ_BLOCK"

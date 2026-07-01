#!/bin/sh
# Called by build_sophon_package.ps1 inside a Docker container on the ext4
# volume to recreate Linux .so symlinks that Git-on-Windows corrupts into
# small text placeholder files.
set -eu

find /workspace/prebuild /workspace/3rd -path '*/lib/*.so*' -type f -size -256c | while IFS= read -r link; do
    target=$(cat "$link" 2>/dev/null || true)
    case "$target" in
        *.so|*.so.*)
            dir=$(dirname "$link")
            if [ -e "$dir/$target" ]; then
                rm -f "$link"
                ln -s "$target" "$link"
                echo "  $link -> $target"
            fi
            ;;
    esac
done
echo "Symlink restoration done"

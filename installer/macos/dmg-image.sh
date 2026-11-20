#!/usr/bin/env bash
# dmg-image.sh — creates, attaches and ejects the disk images of the release
# scripts, with `diskutil image` instead of `hdiutil`.
#
#   dmg-image.sh create <source-folder> <volume-name> <image.dmg>
#   dmg-image.sh attach <image.dmg> <empty-mount-point>     (read-only, hidden)
#   dmg-image.sh eject  <mount-point>                       (no-op if nothing is mounted there)
#
# Exit codes, one per case (a tool error must never look like success):
#   0  done
#   1  diskutil failed with an error that is not retried (its text is printed)
#   2  wrong call (arguments, missing source, a source that is an app bundle
#      itself, bad tuning value)
#   3  create: the build volume was busy in every attempt
#   4  diskutil said OK, but there is no image / nothing is mounted
#   5  eject: still mounted after eject and eject force
#
# Why diskutil (measured on macOS 27.2.1):
#  - `hdiutil create -volname … -format …` prints "is deprecated. Please use
#    'diskutil image create from/blank …' instead" on every call.
#  - `diskutil image create from` gives the same kind of image (UDZO, GUID
#    partition map, APFS, same volume name and tree) and speaks English on a
#    German macOS, so one error can be told from the others by its text.
#    It packs harder (zlib level), so the image is smaller than before.
#
# Why a retry, and only for this one error: right after building, the tool
# ejects its own build volume. When something still holds that volume (on
# a busy machine diskarbitrationd may still be checking the fresh file
# system), the eject fails with EBUSY and the whole create fails. hdiutil said
# "create failed - Resource busy" after about 28 s; diskutil says "Disk image
# operation failed with error code 156" at once. A process with its working
# directory in the build volume reproduces it every time; the next attempt
# works. So create tries again up to DMG_IMAGE_TRIES times with
# DMG_IMAGE_RETRY_PAUSE seconds in between, says so on every failed attempt,
# and fails with exit 3 after the last one. Every other error fails at once.
#
# Two more things diskutil does differently, both handled here:
#  - its progress "[n % completed]" goes to STDERR, with a NO-BREAK SPACE
#    (U+00A0) before "%", and an error follows the last token on the same
#    line; the filter below drops only the progress tokens;
#  - it mounts its build volume at <user TEMP>/<UUID> (getconf
#    DARWIN_USER_TEMP_DIR; $TMPDIR does not move it) and leaves that folder
#    behind, empty, after every create, good or failed. create removes
#    exactly the folders that appeared during its own call: UUID-named, real
#    folder, empty, not a mount point, with rmdir only. While another
#    `diskutil image create` runs on this machine, it leaves them and says so.
#
# Nothing here is silenced: no -quiet, every failure prints the tool's text.
#
# Tuning (for probes and tests; the defaults are what a release build uses):
#   DMG_IMAGE_TRIES        attempts of create (default 3)
#   DMG_IMAGE_RETRY_PAUSE  seconds between them (default 15)
#   DMG_IMAGE_DISKUTIL     the diskutil to run (default /usr/sbin/diskutil);
#                          announced on stderr when it is not the default
set -euo pipefail

ME="dmg-image.sh"
DEFAULT_DISKUTIL=/usr/sbin/diskutil
DISKUTIL="${DMG_IMAGE_DISKUTIL:-$DEFAULT_DISKUTIL}"
TRIES="${DMG_IMAGE_TRIES:-3}"
PAUSE="${DMG_IMAGE_RETRY_PAUSE:-15}"
BUSY_TEXT='Disk image operation failed with error code 156'

usage() {
    sed -n '3,5p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2
    exit 2
}

[[ "$TRIES" =~ ^[1-9][0-9]*$ ]] || { echo "$ME: DMG_IMAGE_TRIES='$TRIES' is no positive number" >&2; exit 2; }
[[ "$PAUSE" =~ ^[0-9]+$ ]] || { echo "$ME: DMG_IMAGE_RETRY_PAUSE='$PAUSE' is no number" >&2; exit 2; }
[[ -x "$DISKUTIL" ]] || { echo "$ME: $DISKUTIL not found or not executable" >&2; exit 2; }
[[ "$DISKUTIL" == "$DEFAULT_DISKUTIL" ]] || echo "$ME: diskutil is $DISKUTIL (DMG_IMAGE_DISKUTIL)" >&2

# True when something is mounted AT $1 (it is a directory on another device
# than its parent).
is_mount_point() {
    [[ -d "$1" && ! -L "$1" ]] || return 1
    [[ "$(stat -f %d "$1")" != "$(stat -f %d "$1/..")" ]]
}

# Drops diskutil's progress tokens, keeps everything else (one line each).
strip_progress() {
    tr '\r' '\n' | LC_ALL=C sed -E 's/\[[0-9]+[^]]*% completed\] ?//g' \
        | LC_ALL=C grep -v '^[[:space:]]*$' || true
}

cmd_create() {
    [[ $# -eq 3 ]] || usage
    local src="$1" vol="$2" out="$3"
    [[ -d "$src" ]] || { echo "$ME: source folder $src does not exist" >&2; exit 2; }
    # diskutil puts the CONTENTS of the source folder into the image. For an
    # app bundle that is Contents/ at the volume root and no .app at all,
    # while `hdiutil create -srcfolder X.app` put X.app itself there - a
    # plain swap of the command gives an image without a startable app,
    # with exit 0. So a source that is a bundle itself is a wrong call.
    if [[ "${src%/}" == *.app || -f "$src/Contents/Info.plist" ]]; then
        echo "$ME: $src is an app bundle itself; diskutil would put only its contents into the" \
             "image - pass the folder that CONTAINS the app" >&2
        exit 2
    fi
    [[ -n "$vol" ]] || { echo "$ME: empty volume name" >&2; exit 2; }
    [[ -d "$(dirname "$out")" ]] || { echo "$ME: folder of $out does not exist" >&2; exit 2; }

    local user_temp parent_dev
    user_temp="$(getconf DARWIN_USER_TEMP_DIR)"
    user_temp="${user_temp%/}"
    [[ -n "$user_temp" && -d "$user_temp" ]] \
        || { echo "$ME: user TEMP not readable (getconf DARWIN_USER_TEMP_DIR: '$user_temp')" >&2; exit 1; }
    parent_dev="$(stat -f %d "$user_temp")"

    uuid_dirs() {
        ls -A "$user_temp" 2>/dev/null \
            | LC_ALL=C grep -E '^[0-9A-F]{8}-([0-9A-F]{4}-){3}[0-9A-F]{12}$' | LC_ALL=C sort || true
    }
    sweep() {  # $1 = uuid_dirs before the call
        local d p
        while IFS= read -r d; do
            [[ -n "$d" ]] || continue
            p="$user_temp/$d"
            if pgrep -qf 'diskutil image create'; then
                echo "$ME: WARNING: left $p - another diskutil image create is running" >&2
            elif [[ -d "$p" && ! -L "$p" && -z "$(ls -A "$p" 2>/dev/null)" \
                    && "$(stat -f %d "$p")" == "$parent_dev" ]] && rmdir "$p" 2>/dev/null; then
                :
            else
                echo "$ME: WARNING: left $p - not an empty folder, or still mounted" >&2
            fi
        done < <(LC_ALL=C comm -13 <(printf '%s\n' "$1") <(uuid_dirs))
    }

    local try rc err before
    for (( try = 1; ; try++ )); do
        rm -f "$out"
        before="$(uuid_dirs)"
        err="$("$DISKUTIL" image create from --format UDZO --volumeName "$vol" \
                 "$src" "$out" 2>&1 >/dev/null)" && rc=0 || rc=$?
        sweep "$before"
        err="$(printf '%s' "$err" | strip_progress)"
        [[ -n "$err" ]] && printf '%s\n' "$err" | sed 's/^/    diskutil: /' >&2
        if (( rc == 0 )); then
            [[ -s "$out" ]] || { echo "$ME: ERROR: diskutil image create said OK, but $out is missing or empty" >&2; exit 4; }
            (( try == 1 )) || echo "$ME: create made it in attempt $try/$TRIES" >&2
            return 0
        fi
        if ! grep -qF "$BUSY_TEXT" <<<"$err"; then
            echo "$ME: ERROR: diskutil image create failed (exit $rc) - not retried, only \"error code 156\" (busy) is" >&2
            exit 1
        fi
        if (( try >= TRIES )); then
            echo "$ME: ERROR: diskutil image create: build volume busy (error code 156) in all $TRIES attempts" >&2
            exit 3
        fi
        echo "$ME: WARNING: create attempt $try/$TRIES: build volume busy (error code 156) - again in $PAUSE s" >&2
        sleep "$PAUSE"
    done
}

cmd_attach() {
    [[ $# -eq 2 ]] || usage
    local img="$1" mp="$2" out rc
    [[ -f "$img" ]] || { echo "$ME: image $img does not exist" >&2; exit 2; }
    [[ -d "$mp" && -z "$(ls -A "$mp")" ]] || { echo "$ME: mount point $mp is no empty folder" >&2; exit 2; }
    is_mount_point "$mp" && { echo "$ME: something is already mounted at $mp" >&2; exit 2; }
    out="$("$DISKUTIL" image attach --readOnly --nobrowse --mountPoint "$mp" "$img" 2>&1)" && rc=0 || rc=$?
    if (( rc != 0 )); then
        printf '%s\n' "$out" | sed 's/^/    diskutil: /' >&2
        echo "$ME: ERROR: diskutil image attach $img failed (exit $rc)" >&2
        exit 1
    fi
    is_mount_point "$mp" || {
        printf '%s\n' "$out" | sed 's/^/    diskutil: /' >&2
        echo "$ME: ERROR: diskutil image attach said OK, but nothing is mounted at $mp" >&2
        exit 4
    }
}

cmd_eject() {
    [[ $# -eq 1 ]] || usage
    local mp="$1" out rc
    is_mount_point "$mp" || return 0
    out="$("$DISKUTIL" eject "$mp" 2>&1)" && rc=0 || rc=$?
    is_mount_point "$mp" || return 0
    printf '%s\n' "$out" | sed 's/^/    diskutil: /' >&2
    echo "$ME: WARNING: diskutil eject $mp (exit $rc) left it mounted - trying eject force" >&2
    out="$("$DISKUTIL" eject force "$mp" 2>&1)" && rc=0 || rc=$?
    is_mount_point "$mp" || return 0
    printf '%s\n' "$out" | sed 's/^/    diskutil: /' >&2
    echo "$ME: ERROR: $mp is still mounted after eject force (exit $rc)" >&2
    exit 5
}

[[ $# -ge 1 ]] || usage
cmd="$1"; shift
case "$cmd" in
    create) cmd_create "$@" ;;
    attach) cmd_attach "$@" ;;
    eject)  cmd_eject "$@" ;;
    *)      usage ;;
esac

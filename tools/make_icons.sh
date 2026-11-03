#!/usr/bin/env bash
# Иконки из res/freeturn-logo-512.png. Нужен ImageMagick 7 (magick).
set -euo pipefail
cd "$(dirname "$0")/../res"
src=freeturn-logo-512.png
out=icons
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

app_sizes=(16 20 24 32 48 256)
tray_sizes=(16 20 24 32 48)
canvas=384

ico() {
    local base=$1 name=$2
    shift 2
    local files=()
    for s in "$@"; do
        magick "$base" -filter Lanczos -resize "${s}x${s}" -strip "PNG32:$tmp/$name-$s.png"
        files+=("$tmp/$name-$s.png")
    done
    magick "${files[@]}" "$out/$name.ico"
}

# В трее только знак F: фон логотипа вырезаем по расстоянию до его цвета, тени ленты остаются.
magick "$src" \( +clone -fill "rgb(56,47,101)" -colorize 100 \) -compose difference -composite \
    -colorspace gray -level 3%,7% "$tmp/alpha.png"
magick "$src" "$tmp/alpha.png" -alpha off -compose copy_opacity -composite -trim +repage \
    -compose over -background none -gravity center -extent "${canvas}x${canvas}" "$tmp/glyph.png"

ico "$src" freeturn "${app_sizes[@]}"

magick "$tmp/glyph.png" -modulate 72,0 "$tmp/off.png"
ico "$tmp/off.png" tray-off "${tray_sizes[@]}"

magick "$tmp/glyph.png" -channel RGB -colorspace gray -colorspace sRGB +level-colors "#4a3600","#ffd84a" +channel \
    "$tmp/connecting.png"
ico "$tmp/connecting.png" tray-connecting "${tray_sizes[@]}"

ico "$tmp/glyph.png" tray-on "${tray_sizes[@]}"

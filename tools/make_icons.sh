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

# Значок приложения - знак без подложки, как у остальных иконок Windows. С 20 px (Пуск, панель
# задач) справа внизу кругляш стадии; на 16 px буква уже не читается.
# Тень внутри петли - сплошная: полупрозрачная на светлом фоне идёт полосами.
stage=β
magick "$tmp/alpha.png" -alpha off \( +clone -threshold 15% -morphology erode disk:2 \) -compose lighten -composite \
    "$tmp/alpha-solid.png"
magick "$src" "$tmp/alpha-solid.png" -alpha off -compose copy_opacity -composite -trim +repage \
    -compose over -background none -gravity center -extent "${canvas}x${canvas}" "$tmp/solid.png"
magick -size "${canvas}x${canvas}" xc:black -fill white -draw "circle 286,286 286,174" -negate "$tmp/cut.png"
magick "$tmp/solid.png" \( "$tmp/solid.png" -alpha extract "$tmp/cut.png" -compose multiply -composite \) \
    -alpha off -compose copy_opacity -composite \
    \( -size "${canvas}x${canvas}" xc:none -fill "#7b61d1" -draw "circle 286,286 286,188" \
       -font Noto-Sans-Bold -pointsize 150 -fill white -gravity center -annotate +94+94 "$stage" \) \
    -compose over -composite "$tmp/badged.png"
app=()
for s in "${app_sizes[@]}"; do
    base=$tmp/solid.png
    ((s >= 20)) && base=$tmp/badged.png
    magick "$base" -filter Lanczos -resize "${s}x${s}" -strip "PNG32:$tmp/freeturn-$s.png"
    app+=("$tmp/freeturn-$s.png")
done
magick "${app[@]}" "$out/freeturn.ico"

magick "$tmp/glyph.png" -modulate 72,0 "$tmp/off.png"
ico "$tmp/off.png" tray-off "${tray_sizes[@]}"

magick "$tmp/glyph.png" -channel RGB -colorspace gray -colorspace sRGB +level-colors "#4a3600","#ffd84a" +channel \
    "$tmp/connecting.png"
ico "$tmp/connecting.png" tray-connecting "${tray_sizes[@]}"

ico "$tmp/glyph.png" tray-on "${tray_sizes[@]}"

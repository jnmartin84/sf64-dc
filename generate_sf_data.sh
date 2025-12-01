SF_DATA_PATH=sf_data
SF_MUSIC_PATH=music

OUT="./sf64_ost_seqid.tgz"
URL="https://archive.org/download/sf64_ost_seqid/sf64_ost_seqid.tgz"


# Check if directory exists
if [ ! -d "$SF_MUSIC_PATH" ]; then
    echo "Music does not exist, downloading..."
    mkdir -p $SF_MUSIC_PATH
    cd $SF_MUSIC_PATH
    if command -v wget >/dev/null 2>&1; then
        wget -O "$OUT" "$URL"
        tar xzf "$OUT"
        rm "$OUT"
        for f in *.wav; do
            [ -e "$f" ] || continue
            adpout="${f%.wav}.adp"
            echo "Converting: $f -> $adpout"
            $KOS_BASE/utils/wav2adpcm/wav2adpcm -n -i -t "$f" "$adpout"
        done
        rm *.wav
    elif command -v curl >/dev/null 2>&1; then
        curl -L -o "$OUT" "$URL"
        tar xzf "$OUT"
        rm "$OUT"
        for f in *.wav; do
            [ -e "$f" ] || continue
            adpout="${f%.wav}.adp"
            echo "Converting: $f -> $adpout"
            $KOS_BASE/utils/wav2adpcm/wav2adpcm -n -i -t "$f" "$adpout"
        done
        rm "./*.wav"
    else
        echo "Error: neither wget nor curl found on this system."
        echo "Your build will play music with CPU mixer. This is slow."
    fi
    cd ..
fi

mkdir -p $SF_DATA_PATH

# dreamcast replacement graphics

$KOS_BASE/utils/pvrtex/pvrtex -i assets/dreamcast/dclogo.png -o dclogo.tex -f ARGB1555 -s 128
$KOS_BASE/utils/pvrtex/pvrtex -i assets/dreamcast/aVsDCConsoleTex.png -o console.tex -f ARGB1555 -s 256
$KOS_BASE/utils/bin2o/bin2o dclogo.tex dclogo build/dclogo.o
$KOS_BASE/utils/bin2o/bin2o console.tex vs_dc_console build/dcconsole.o
rm dclogo.tex
rm console.tex

# audio bank / sequence data

cp bin/us/rev1/audio_bank.bin $SF_DATA_PATH/audbank.bin
cp bin/us/rev1/audio_seq.bin $SF_DATA_PATH/audseq.bin
cp bin/us/rev1/audio_table.bin $SF_DATA_PATH/audtable.bin

# asset segments

sh-elf-ld -EL -t -e 0 -Ttext=05000000 build/src/assets/ast_text/ast_text.o -o build/src/assets/ast_text/ast_text.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_text/ast_text.elf $SF_DATA_PATH/text.bin

sh-elf-ld -EL -t -e 0 -Ttext=01000000 build/src/assets/ast_common/ast_common.o -o build/src/assets/ast_common/ast_common.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_common/ast_common.elf $SF_DATA_PATH/common.bin

sh-elf-ld -EL -t -e 0 -Ttext=02000000 build/src/assets/ast_bg_space/ast_bg_space.o -o build/src/assets/ast_bg_space/ast_bg_space.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bg_space/ast_bg_space.elf $SF_DATA_PATH/bgspace.bin

sh-elf-ld -EL -t -e 0 -Ttext=02000000 build/src/assets/ast_bg_planet/ast_bg_planet.o -o build/src/assets/ast_bg_planet/ast_bg_planet.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bg_planet/ast_bg_planet.elf $SF_DATA_PATH/bgplanet.bin

sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_arwing/ast_arwing.o -o build/src/assets/ast_arwing/ast_arwing.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_arwing/ast_arwing.elf $SF_DATA_PATH/arwing.bin

sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_landmaster/ast_landmaster.o -o build/src/assets/ast_landmaster/ast_landmaster.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_landmaster/ast_landmaster.elf $SF_DATA_PATH/landmstr.bin

sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_blue_marine/ast_blue_marine.o -o build/src/assets/ast_blue_marine/ast_blue_marine.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_blue_marine/ast_blue_marine.elf $SF_DATA_PATH/blumarin.bin

sh-elf-ld -EL -t -e 0 -Ttext=03000000 build/src/assets/ast_versus/ast_versus.o -o build/src/assets/ast_versus/ast_versus.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_versus/ast_versus.elf $SF_DATA_PATH/versus.bin

sh-elf-ld -EL -t -e 0 -Ttext=04000000 build/src/assets/ast_enmy_planet/ast_enmy_planet.o -o build/src/assets/ast_enmy_planet/ast_enmy_planet.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_enmy_planet/ast_enmy_planet.elf $SF_DATA_PATH/enmyplnt.bin

sh-elf-ld -EL -t -e 0 -Ttext=04000000 build/src/assets/ast_enmy_space/ast_enmy_space.o -o build/src/assets/ast_enmy_space/ast_enmy_space.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_enmy_space/ast_enmy_space.elf $SF_DATA_PATH/enmyspce.bin

sh-elf-ld -EL -t -e 0 -Ttext=0E000000 build/src/assets/ast_great_fox/ast_great_fox.o -o build/src/assets/ast_great_fox/ast_great_fox.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_great_fox/ast_great_fox.elf $SF_DATA_PATH/greatfox.bin

sh-elf-ld -EL -t -e 0 -Ttext=0F000000 build/src/assets/ast_star_wolf/ast_star_wolf.o -o build/src/assets/ast_star_wolf/ast_star_wolf.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_star_wolf/ast_star_wolf.elf $SF_DATA_PATH/starwolf.bin

sh-elf-ld -EL -t -e 0 -Ttext=0D000000 build/src/assets/ast_allies/ast_allies.o -o build/src/assets/ast_allies/ast_allies.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_allies/ast_allies.elf $SF_DATA_PATH/allies.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_corneria/ast_corneria.o -o build/src/assets/ast_corneria/ast_corneria.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_corneria/ast_corneria.elf $SF_DATA_PATH/corneria.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_meteo/ast_meteo.o -o build/src/assets/ast_meteo/ast_meteo.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_meteo/ast_meteo.elf $SF_DATA_PATH/meteo.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_titania/ast_titania.o -o build/src/assets/ast_titania/ast_titania.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_titania/ast_titania.elf $SF_DATA_PATH/titania.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_x/ast_sector_x.o -o build/src/assets/ast_sector_x/ast_sector_x.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_x/ast_sector_x.elf $SF_DATA_PATH/sectorx.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_z/ast_sector_z.o -o build/src/assets/ast_sector_z/ast_sector_z.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_z/ast_sector_z.elf $SF_DATA_PATH/sectorz.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_aquas/ast_aquas.o -o build/src/assets/ast_aquas/ast_aquas.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_aquas/ast_aquas.elf $SF_DATA_PATH/aquas.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_area_6/ast_area_6.o -o build/src/assets/ast_area_6/ast_area_6.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_area_6/ast_area_6.elf $SF_DATA_PATH/area6.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_venom_1/ast_venom_1.o -o build/src/assets/ast_venom_1/ast_venom_1.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_venom_1/ast_venom_1.elf $SF_DATA_PATH/venom1.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_venom_2/ast_venom_2.o -o build/src/assets/ast_venom_2/ast_venom_2.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_venom_2/ast_venom_2.elf $SF_DATA_PATH/venom2.bin

sh-elf-ld -EL -t -e 0 -Ttext=09000000 build/src/assets/ast_ve1_boss/ast_ve1_boss.o -o build/src/assets/ast_ve1_boss/ast_ve1_boss.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ve1_boss/ast_ve1_boss.elf $SF_DATA_PATH/ve1boss.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_bolse/ast_bolse.o -o build/src/assets/ast_bolse/ast_bolse.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_bolse/ast_bolse.elf $SF_DATA_PATH/bolse.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_fortuna/ast_fortuna.o -o build/src/assets/ast_fortuna/ast_fortuna.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_fortuna/ast_fortuna.elf $SF_DATA_PATH/fortuna.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_sector_y/ast_sector_y.o -o build/src/assets/ast_sector_y/ast_sector_y.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_sector_y/ast_sector_y.elf $SF_DATA_PATH/sectory.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_solar/ast_solar.o -o build/src/assets/ast_solar/ast_solar.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_solar/ast_solar.elf $SF_DATA_PATH/solar.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_zoness/ast_zoness.o -o build/src/assets/ast_zoness/ast_zoness.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_zoness/ast_zoness.elf $SF_DATA_PATH/zoness.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_katina/ast_katina.o -o build/src/assets/ast_katina/ast_katina.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_katina/ast_katina.elf $SF_DATA_PATH/katina.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_macbeth/ast_macbeth.o -o build/src/assets/ast_macbeth/ast_macbeth.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_macbeth/ast_macbeth.elf $SF_DATA_PATH/macbeth.bin

sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_warp_zone/ast_warp_zone.o -o build/src/assets/ast_warp_zone/ast_warp_zone.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_warp_zone/ast_warp_zone.elf $SF_DATA_PATH/warpzone.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_title/ast_title.o -o build/src/assets/ast_title/ast_title.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_title/ast_title.elf $SF_DATA_PATH/title.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_map/ast_map.o -o build/src/assets/ast_map/ast_map.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_map/ast_map.elf $SF_DATA_PATH/map.bin

sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_option/ast_option.o -o build/src/assets/ast_option/ast_option.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_option/ast_option.elf $SF_DATA_PATH/option.bin

sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_vs_menu/ast_vs_menu.o -o build/src/assets/ast_vs_menu/ast_vs_menu.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_vs_menu/ast_vs_menu.elf $SF_DATA_PATH/vsmenu.bin

sh-elf-ld -EL -t -e 0 -Ttext=09000000 build/src/assets/ast_font_3d/ast_font_3d.o -o build/src/assets/ast_font_3d/ast_font_3d.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_font_3d/ast_font_3d.elf $SF_DATA_PATH/font3d.bin

sh-elf-ld -EL -t -e 0 -Ttext=0C000000 build/src/assets/ast_andross/ast_andross.o -o build/src/assets/ast_andross/ast_andross.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_andross/ast_andross.elf $SF_DATA_PATH/andross.bin

sh-elf-ld -EL -t -e 0 -Ttext=0F000000 build/src/assets/ast_logo/ast_logo.o -o build/src/assets/ast_logo/ast_logo.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_logo/ast_logo.elf $SF_DATA_PATH/logo.bin

sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_ending/ast_ending.o -o build/src/assets/ast_ending/ast_ending.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending/ast_ending.elf $SF_DATA_PATH/ending.bin

sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_award_front/ast_ending_award_front.o -o build/src/assets/ast_ending_award_front/ast_ending_award_front.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_award_front/ast_ending_award_front.elf $SF_DATA_PATH/awdfront.bin

sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_award_back/ast_ending_award_back.o -o build/src/assets/ast_ending_award_back/ast_ending_award_back.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_award_back/ast_ending_award_back.elf $SF_DATA_PATH/awdback.bin

sh-elf-ld -EL -t -e 0 -Ttext=08000000 build/src/assets/ast_ending_expert/ast_ending_expert.o -o build/src/assets/ast_ending_expert/ast_ending_expert.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_ending_expert/ast_ending_expert.elf $SF_DATA_PATH/endexprt.bin

sh-elf-ld -EL -t -e 0 -Ttext=06000000 build/src/assets/ast_training/ast_training.o -o build/src/assets/ast_training/ast_training.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_training/ast_training.elf $SF_DATA_PATH/training.bin

sh-elf-ld -EL -t -e 0 -Ttext=07000000 build/src/assets/ast_7_ti_1/ast_7_ti_1.o -o build/src/assets/ast_7_ti_1/ast_7_ti_1.elf
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_7_ti_1/ast_7_ti_1.elf $SF_DATA_PATH/7ti1.bin

#exit 0

sh-elf-ld -EL -t -Ttext=07000000 -Tdata=07000000 build/src/assets/ast_7_ti_2/ast_7_ti_2.o -o build/src/assets/ast_7_ti_2/ast_7_ti_2.elf --no-check-sections --allow-shlib-undefined -r
sh-elf-ld -EL -t -Ttext=09000000 -Tdata=09000000 build/src/assets/ast_9_ti/ast_9_ti.o -o build/src/assets/ast_9_ti/ast_9_ti.elf --no-check-sections --allow-shlib-undefined -r
sh-elf-ld -EL -t -Ttext=08000000 -Tdata=08000000 build/src/assets/ast_8_ti/ast_8_ti.o -o build/src/assets/ast_8_ti/ast_8_ti.elf --no-check-sections --allow-shlib-undefined -r

sh-elf-ld -EL -t -R build/src/assets/ast_8_ti/ast_8_ti.elf -Ttext=09000000 build/src/assets/ast_9_ti/ast_9_ti.o -o build/src/assets/ast_9_ti/ast_9_ti.elf --no-check-sections

sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_7_ti_2/ast_7_ti_2.elf -Ttext=08000000 build/src/assets/ast_8_ti/ast_8_ti.o -o build/src/assets/ast_8_ti/ast_8_ti.elf --no-check-sections -z muldefs

sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_8_ti/ast_8_ti.elf -Ttext=07000000 build/src/assets/ast_7_ti_2/ast_7_ti_2.o -o build/src/assets/ast_7_ti_2/ast_7_ti_2.elf --no-check-sections -z muldefs

sh-elf-ld -EL -t -R build/src/assets/ast_9_ti/ast_9_ti.elf -R build/src/assets/ast_8_ti/ast_8_ti.elf -R build/src/assets/ast_9_ti/ast_9_ti.elf -Ttext=0A000000 build/src/assets/ast_A_ti/ast_A_ti.o -o build/src/assets/ast_A_ti/ast_A_ti.elf --no-check-sections -z muldefs

sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_A_ti/ast_A_ti.elf $SF_DATA_PATH/ati.bin
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_9_ti/ast_9_ti.elf $SF_DATA_PATH/9ti.bin
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_8_ti/ast_8_ti.elf $SF_DATA_PATH/8ti.bin
sh-elf-objcopy -O binary --only-section=.data --only-section=.bss build/src/assets/ast_7_ti_2/ast_7_ti_2.elf $SF_DATA_PATH/7ti2.bin


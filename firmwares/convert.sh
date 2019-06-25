uno=""
mega=""
for f in *.hex; do 
    echo "Processing $f file.."; 
    objcopy $f ${f%.*}.bin -O binary -I ihex --gap-fill 0xff
    if [[ $f == *"Uno"* ]]; then
      uno="$uno`xxd -i -c 16 ${f%.*}.bin`\n" 
    fi
    if [[ $f == *"Mega"* ]]; then
      mega="$mega`xxd -i -c 16 ${f%.*}.bin`\n" 
    fi
done
echo -e "${mega}" > ../bootloaders-mega.h
echo -e "${uno}" > ../bootloaders-uno.h
sed -i -e 's/unsigned char/const byte/g' -e 's/HoodLoader2_0_5_Uno_//g' -e 's/bin\[\]/bin [] PROGMEM/g'  ../bootloaders-uno.h
sed -i -e 's/unsigned char/const byte/g' -e 's/HoodLoader2_0_5_Mega_//g' -e 's/bin\[\]/bin [] PROGMEM/g' ../bootloaders-mega.h
# 將screen.cpp 及 main.cpp 的文本提出來 以UTF8的編碼型態 儲存到$src裡
Get-Content ..\src\screen.cpp,..\src\main.cpp -Raw -Encoding UTF8 > temp.cpp
$src = Get-Content temp.cpp -Encoding UTF8 -Raw

$uniEnc = [System.Text.Encoding]::Unicode

# 將$src裡不是ASCII碼的挑選出來，再來提取Match物件的Value，接著按照順序排列，最後去除重複的
$uniChars = [System.Text.RegularExpressions.Regex]::Matches($src, '[^\x0a-\x7f]') | 
    ForEach-Object { $_.Value } | Sort-Object | Get-Unique

Write-Host "發現中文字元: "

# 將每個字符用 "，" 隔開
$uniChars -join ', ' 

(@("32-128") + ($uniChars | ForEach-Object { '$' + ([Uint32]([char]$_)).ToString('X4') })) -join ",`n" | Out-File ".\build\chinese1.map" -Encoding utf8
# 依列舉字元表，製作 u8g2_font_unifont_t_chinese1 陣列宣告
# -b Font build mode, 0: proportional, 1: common height, 2: monospace, 3: multiple of 8
# -f Font format, 0: ucglib font, 1: u8g2 font, 2: u8g2 uncompressed 8x8 font (enforces -b 3)
# -M Read Unicode ASCII mapping from file 'mapname'


& .\bdfconv\bdfconv.exe -v ./bdf/WenQuanYiZenHeiMono-16.bdf -b 0 -f 1 -M ./build/chinese1.map -d ./bdf/WenQuanYiZenHeiMono-16.bdf -n cht_font_16 -o cht_font_16.h
# 找到 .pio 下的 u8g2_fonts.c，將 u8g2_font_unifont_t_chinese1 內容換掉

& .\bdfconv\bdfconv.exe -v ./bdf/WenQuanYiZenHeiMono-24.bdf -b 0 -f 1 -M ./build/chinese1.map -d ./bdf/WenQuanYiZenHeiMono-24.bdf -n cht_font_24 -o cht_font_24.h
# 找到 .pio 下的 u8g2_fonts.c，將 u8g2_font_unifont_t_chinese1 內容換掉

& .\bdfconv\bdfconv.exe -v ./bdf/WenQuanYiZenHeiMono-32.bdf -b 0 -f 1 -M ./build/chinese1.map -d ./bdf/WenQuanYiZenHeiMono-32.bdf -n cht_font_32 -o cht_font_32.h



# LR FX Gen 2 — Plate / Hall / Delay / Harmony / SPX Plate (Standalone + VST3)

## Kontrol
| Kontrol | Fungsi |
|---|---|
| LCD | Tipe FX, nomor + nama preset (`2 NamaPreset`), 4 LED tempo (merah, hijau, hijau, hijau), BPM |
| Down / Up | Home: pilih preset (lalu **klik kiri knop TYPE/PROGRAM** untuk load). Menu: geser baris parameter. Store: pilih slot |
| Utility | Panel Audio Input / Output (sama seperti LR PRO FX): pilih None / Channel 1 (L) / Channel 2 (R) untuk Input 1, Input 2, Output 1, Output 2. Tombol BACK / Menu / Exit menutup panel |
| Menu | Plate/Hall/SPX Plate: Decay, PreDelay, Room, Diffusion, Damping, Low Cut, Hi Cut (20–20000 Hz, filter one-pole seperti LR PRO FX; default 80 Hz / 16 kHz). Delay: Time, Feedback, Hpf, Lpf (filter 24 dB/oct, menyaring seluruh echo termasuk echo pertama). Harmony: Key (C–B), Mode (11 pilihan), Sharp (Major/Minor), Tone (Normal/Warm/Bright), Out (level fx, dB). Ubah nilai dengan roll mouse pada knop TYPE/PROGRAM (atau drag) |
| Exit | Kembali ke tampilan awal |
| Store | Popup nama preset; klik kiri knop TYPE/PROGRAM (atau Enter) untuk menyimpan ke slot (maks. 10) |
| TYPE/PROGRAM | Home: roll mouse = pilih Plate / Hall / Delay / Harmony / SPX Plate |
| INPUT / MIX / OUTPUT | Level masuk / campuran dry-wet / level keluar |
| REV EQ | Low shelf ±18 dB @ 800 Hz, High shelf ±18 dB @ 8 kHz (pada sinyal efek) |
| TAP | Delay: tap tempo (klik atau **Spasi**), Delay Time = interval tap. Harmony: ganti **Sharp** Major <-> Minor. Tipe lain: tidak berfungsi |

## Tipe SPX PLATE
Reverb plate bergaya digital 90-an (karakter mengacu ke Yamaha SPX): input mono, pre-delay, diffuser, tank Dattorro termodulasi dengan banyak titik tap output. Ekornya rapat tanpa early reflection yang menonjol, HF meluruh lebih cepat dari mid (atur dengan **Damping**), lebar stereo sedang. Menu sama dengan Plate/Hall (Decay, PreDelay, Room, Diffusion, Damping, Low Cut, Hi Cut).
Titik awal: preset slot 5 "SPX Plate" (Decay 1.8 s, PreDelay 15 ms, Room 50, Diffusion 80, Damping 65, Low Cut 100 Hz, Hi Cut 12 kHz).
Catatan: ini peniruan karakter, bukan salinan algoritma Yamaha. Setel Damping / Decay dengan telinga terhadap rekaman referensi.

## Tipe HARMONY
Mendeteksi nada suara (monofonik, 70–900 Hz), memetakannya ke tangga nada **Key + Major/Minor**, lalu membuat 1–2 suara harmoni diatonis.
Mode: `OCT-`, `5th L`, `3rd L`, `3rd H`, `5th H`, `OCT+` (satu suara) dan `OCT- & OCT+`, `5th L & 3rd H`, `5th L & 5th H`, `3rd L & 3rd H`, `3rd H & 5th H` (dua suara, kiri/kanan).
Terts/kuint mengikuti tangga nada (terts mayor atau minor tergantung nada yang dinyanyikan). Knop **MIX** tetap dry/wet; **Out** = level suara harmoni. Cocok untuk sumber monofonik (vokal / instrumen satu nada).

Preset tersimpan di `presets.xml` (folder data aplikasi pengguna, subfolder `ReverbFX`).
Slot 1–5 berisi preset contoh yang bisa ditimpa. Preset lama otomatis dikonversi (file presets.xml versi 4).

## ASIO (Windows Standalone, latensi rendah)
Build Windows menyertakan driver **ASIO** bila ASIO SDK ditemukan saat build (workflow GitHub Actions mengambilnya otomatis; lihat log `ASIO: AKTIF`). Di Standalone: **Options → Audio/MIDI Settings → Audio device type = ASIO**, pilih perangkat ASIO (atau FlexASIO / ASIO4ALL), lalu perkecil **Audio buffer size** (mis. 64–128 sampel). Tanpa driver ASIO, tipe "Windows Audio (Low Latency Mode)" tetap bisa dipakai. Pengaturan ASIO hanya untuk Standalone; di VST3, latensi diatur oleh DAW.
Cara lain menyediakan SDK: unduh ASIO SDK resmi dari steinberg.net/asiosdk dan taruh di `ThirdParty/asiosdk` (harus berisi `common/iasiodrv.h`), atau set `-DASIO_SDK_DIR=...` saat configure. ASIO SDK berlisensi GPLv3 atau proprietary Steinberg; periksa ketentuannya sebelum mendistribusikan build.

Catatan standalone: input audio dalam keadaan mute secara default (anti-feedback). Buka Utility → Audio/MIDI Settings
lalu hilangkan centang *Mute audio input*.

Lisensi: JUCE 8 memakai lisensi AGPLv3 / komersial — periksa ketentuannya sebelum mendistribusikan plugin.

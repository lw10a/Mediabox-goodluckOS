# mediabox kullanım kılavuzu (Türkçe)

mediabox, 640x480 ekranlı ve oyun kollu bir el konsolunda müzik (MP3) çalar, kitap (PDF) açar ve video (MP4, MKV, AVI)
oynatır. R36S klon (GA36-MB, Allwinner A23) üzerinde goodluckOS ile yazıldı ve orada denendi; SDL2 ve SDL2_ttf olan diğer
32-bit ARM Linux cihazlarında çalışabilir ama denenmedi (`README.md` içindeki Uyumluluk tablosuna bak).

## 1. Kurulum

### 1.1 Uygulamayı derleme (bir kere, GitHub'da)
Bilgisayarında hiçbir şey derlemen gerekmiyor. Derlemeyi GitHub senin yerine yapar.

**A seçeneği - depoyu fork'la (en kolayı)**
1. GitHub'a giriş yap ve mediabox deposunun sağ üstündeki **Fork** düğmesine bas. Kendi kopyan oluşur.
2. Kopyanın **Actions** sekmesini aç. GitHub fork'larda workflow'ları kapalı tutar, o yüzden
   **I understand my workflows, go ahead and enable them** düğmesine bas.
3. Soldan **build-mediabox**'u seç ve **Run workflow**'a bas (iki kez: biri kutuyu açar, biri onaylar).
4. Yeşil tik çıkana kadar bekle. İlk derleme bir iki saat sürebilir, sonrakiler önbellek sayesinde kısadır.
5. Biten çalışmayı aç, en alttaki **Artifacts** bölümünden **mediabox-armhf** dosyasını indir ve zipten çıkar.
   İçinde `mediabox` dosyası var.

**B seçeneği - dosyaları yeni bir depoya yükle**
1. Yeni bir depo oluştur (Private olabilir), örneğin `mediabox`.
2. **Add file > Upload files** ile `Dockerfile`, `src` klasörü, `README.md`, `NOTICE.md`, `mediabox.puppy` ve `docs`
   klasörünü sürükle. `.github` klasörünü de yükle. Web sayfası gizli klasörü kabul etmezse
   **Add file > Create new file** de, dosya adına `.github/workflows/build.yml` yaz ve içine
   `workflow-build.yml.txt` dosyasının içeriğini yapıştır.
3. A seçeneğindeki 3-5. adımlarla devam et.

> Private depolarda aylık ücretsiz Actions dakikası sınırlıdır; ilk derleme bunun büyük kısmını harcayabilir.
> Public depolarda sınır yoktur.
>
> Sonradan güncelleme: kendi deponda `src/mediabox.c` dosyasını (gerekirse `Dockerfile`'ı da) değiştir ya da fork'unu
> orijinal depoyla eşitle, sonra workflow'u tekrar çalıştır.

### 1.2 Konsola kopyalama
1. `/home/player/ports/mediabox/` klasörünü oluştur ve `mediabox` dosyasını içine koy.
   Gerekirse: `chmod +x /home/player/ports/mediabox/mediabox`
2. `mediabox.puppy` dosyasını `/home/player/.local/share/applications/` içine kopyala (`kennel.puppy`'nin yanına).
3. Yeniden başlat ya da menüyü yeniden aç. **Media** kategorisinde uygulama görünür.

### 1.3 Dosyalarını koy
| İçerik | Dahili kart | Harici kart | Türler |
|---|---|---|---|
| Müzik | `/home/player/music` | `/media/external/music` | `.mp3` |
| Kitap | `/home/player/books` | `/media/external/books` | `.pdf` |
| Video | `/home/player/videos` | `/media/external/videos` | `.mp4 .m4v .mov .mkv .avi` |

Alt klasörler kullanılabilir. Müzik klasöründeki video dosyaları da **Müzik** altında listelenir ve video olarak açılır.
Adı `.` ile başlayan dosyalar gizlenir.

## 2. Ana menü
Beş kutu: **Müzik**, **Kitaplar (PDF)**, **Videolar**, **Dil** ve **Yardım**.

| Tuş | İşlev |
|---|---|
| Yukarı / Aşağı | kutu seç |
| **A** | kutuyu aç |
| **Y** | dili değiştir (İngilizce / Türkçe) |
| **Start** | "Şimdi çalıyor" ekranına git (müzik yüklüyse) |
| **B** | uygulamadan çık |

Seçtiğin dil kaydedilir, bir sonraki açılışta da kullanılır. Varsayılan dil İngilizcedir.

## 3. Klasörlerde gezinme
| Tuş | İşlev |
|---|---|
| Yukarı / Aşağı | hareket et |
| **A** | klasörü aç / dosyayı oynat |
| **B** | bir üst klasöre çık (en üstte ana menüye döner) |
| **L / R** | bir sayfa yukarı / aşağı atla |
| **Start** | "Şimdi çalıyor" ekranını aç |

İki kartta da klasör varsa önce **Dahili kart** ya da **Harici kart** seçilir.
Altta çalan müziği gösteren bir çubuk durur, böylece kitap okurken ya da gezinirken dinlemeye devam edebilirsin.

## 4. Müzik çalar
| Tuş | İşlev |
|---|---|
| **A** veya Start | duraklat / devam |
| Sol / Sağ | 10 saniye geri / ileri sar |
| **L** | parçayı başa al (başlangıçtaysa öncekine geç) |
| **R** | sonraki parça |
| **X** | karışık çalma aç/kapat |
| **Y** | tekrar: kapalı, tüm klasör, tek parça |
| **Select** | durdur ve listeyi temizle |
| **B** | listeye dön (müzik çalmaya devam eder) |

Bir dosyayı açınca klasördeki tüm MP3'ler sıraya girer; parça bitince sonraki otomatik başlar.

## 5. Video oynatıcı
| Tuş | İşlev |
|---|---|
| **A** veya Start | duraklat / devam |
| Sol / Sağ | 10 sn sar. **Basılı tutunca hızlanır**: adım 20, 40, sonra 90 sn olur |
| **L / R** | basış başına 1 dakika sar (art arda basışlar toplanır) |
| Yukarı / Aşağı | ses, %5 adımlarla (sistem sesidir, aşağıya bak) |
| **X** | sığdır (siyah bantlar) / doldur (kenarları kırpar) |
| **Y** | açtıktan hemen sonra, "... konumundan devam ediliyor" yazarken: baştan başla |
| **B** veya Select | durdur ve geri dön (konum kaydedilir) |

**Sarma.** Sol/Sağ'ı basılı tuttuğunda görüntü donar, kontrol çubuğu hedef süreyi ve miktarı gösterir
(örneğin `Sar +2:40`). Atlama, tuşu bıraktığında bir kez yapılır (hemen gitmek için **A**'ya bas).

**Kaldığın yerden devam (sadece videolar).** mediabox her videoda nerede kaldığını hatırlar. Bir sonraki açışta oradan
devam eder ve 8 saniye "... konumundan devam ediliyor" yazar; baştan izlemek için **Y**'ye bas. Konum 15 saniyede bir ve
videodan çıkarken kaydedilir, sonuna kadar izleyince silinir (son 30 saniye sonuna sayılır). Video listesinde kayıtlı
süre dosya adının sağında görünür. Son 128 video hatırlanır.

**Ses.** Yukarı/Aşağı konsolun gerçek (ALSA) sesini değiştirir, yan ses tuşlarının değiştirdiği aynı sestir; kontrol
çubuğundaki sayı hep eşleşir. Ses sistemine erişilemezse uygulama kendi sesine (%0-200) döner. Sayı yan tuşları
izlemiyorsa, `settings.cfg` dosyasına bir `mixer=ADI` satırı ekleyip tuşların hangi mixer kontrolünü kullandığını
söyle (adları `amixer scontrols` ile listele, örneğin `mixer=Headphone`).

Kontrol çubuğu her tuşa basışta birkaç saniye ve duraklatılmışken görünür. Başlık, ilerleme çubuğu, süre, ses,
görünüm modu ve **Atlanan** (işlemci yetişemediği için atlanan kare sayısı) bilgisini gösterir.

### Videoların akıcı çalması için
Konsol videoyu sadece işlemciyle çözer (donanım çözücü yok). Büyük ya da yüksek bitrate'li videoları önce bilgisayarda
dönüştür. [FFmpeg](https://ffmpeg.org) ile:

```
ffmpeg -i giris.mkv -vf "scale=-2:360" -c:v libx264 -profile:v main -preset slow -crf 25 -r 30 -c:a aac -b:a 128k -ac 2 cikis.mp4
```

İyi hedef: **360p-480p**, H.264 Baseline ya da Main, 24-30 fps, yaklaşık 800-1200 kbps video, AAC stereo ses.
Desteklenmeyenler: HEVC/H.265, VP9, AV1. **Atlanan** sayısı hızla artıyorsa çözünürlüğü ya da bitrate'i düşür.

## 6. PDF okuyucu
| Tuş | İşlev |
|---|---|
| **L / R** | önceki / sonraki sayfa |
| **L2 / R2** | 10 sayfa geri / ileri |
| **A** | yakınlaştırma: sayfaya sığdır, genişliğe sığdır, %150, %200, %300 |
| **X** | "sayfaya sığdır"a dön |
| Yön tuşları / sol çubuk | büyütülmüş sayfayı kaydır (kenarda sayfayı çevirir) |
| **B** | kapat (kaldığın sayfa hatırlanır) |

Şifreli PDF'ler açılamaz. Çok büyük sayfalar bellek için sınırlı çözünürlükte çizilir.

## 7. Sorun giderme
| Sorun | Ne yapmalı |
|---|---|
| "Klasör yok" | 1.3 bölümündeki klasörü oluştur (adlar küçük harf). |
| Video takılıyor | 360p-480p'ye dönüştür (5. bölüm). **Atlanan** sayacına bak. |
| Uygulamadaki ses yan tuşlarla uyuşmuyor | `settings.cfg` dosyasına `mixer=ADI` ekle (5. bölüm, Ses). |
| "Bu video biçimi desteklenmiyor" | Video HEVC/VP9/AV1. H.264 olarak yeniden kodla. |
| Videoda ses yok | Ses kodeği desteklenmiyor olabilir (ör. DTS). AAC'ye dönüştür. |
| Dil yanlış | Ana menüde **Y**'ye bas. |
| Firefox indirirken uyarı veriyor | Dosya imzasız ve az indirilen bir ikili olduğu için itibar uyarısı çıkar. Kendi deponda derle (1.1) ve yalnızca oradan kullan. |
| Derleme hata veriyor | Actions'ta başarısız çalışmayı aç, log'un son 40 satırını oku. |

## 8. Uygulamanın yazdığı dosyalar
- `/home/player/.config/mediabox/settings.cfg` - dil (`lang=en|tr`) ve isteğe bağlı `mixer=ADI`
- `/home/player/.config/mediabox/progress.tsv` - her PDF'in son sayfası
- `/home/player/.config/mediabox/video_progress.tsv` - her videonun kaldığın yeri

Uygulamayı sıfırlamak için bunları sil.

# AI Kod Araçları Karşılaştırması — Proje Özeti

**Amaç:** 4 AI kod aracının (Claude Code · Cursor · Codex · Antigravity) **aynı programlama problemlerini aynı prompt'la** çözdürülüp; ürettikleri kodun **doğruluk · kalite · güvenlik · verimlilik · süreç** boyutlarında karşılaştırılması.

**Yöntem:** Manuel yürütme (abonelik/ücretsiz erişim, ~$0 maliyet). Doğruluk, SJTU/ACMOJ online judge'a erişim olmadığı için **kendi yerel checker'larımız + kendi gizli testlerimizle** ölçülür. Temel: ProjDevBench (arXiv 2602.01655). Tüm ölçümler WSL/Linux'ta, **tüm araçlar aynı ortamda** (göreceli kıyas adil).

**Durum:** ✅ **12 problem tamam** — 48 koşu (12 problem × 4 araç). Sıradaki: istatistik + makale.

---

## 1. Değerlendirilen araçlar

| Araç | Model | Arayüz |
|------|-------|--------|
| Claude Code | opus-4.8-high | CLI (terminal) |
| Cursor | opus-4.8-high | IDE (GUI) |
| Codex | gpt-5.4-high | CLI/IDE |
| Antigravity | gemini-3.1-pro-high | IDE (GUI) |

Her araca **tek başlangıç promptu** verildi (kelimesi kelimesine aynı), araç **otonom** çalıştı, **müdahale edilmedi** (düzeltici prompt yasak → kontaminasyon önlemi). Araç "bitti" dediğinde sonuç olduğu gibi kaydedildi (kendini doğrulayamama bir **bulgu**dur).

---

## 2. Problemler — zorluk · kategori · nasıl doğrulandı · sonuç

> Kolaydan zora. Her problem 4 araçla çözüldü. **Framework** = agent verilen scaffold'daki tek dosyayı doldurur; **standalone** = sıfırdan yazar.

| # | Problem | Zorluk | Kategori | Doğrulama (oracle) yöntemi | Sonuç |
|:---:|---|:---:|---|---|---|
| 001 | A+B (pilot) | 🟢 | Isınma | kendi testlerimiz | 4 araç 6/6 |
| 009 | STLite Vector | 🟢 | Dinamik dizi | 7 canonical test (gerçek çıktıyla kıyas) | 4 araç 7/7 |
| 020 | Buddy Algorithm | 🟢 | Bellek ayırıcı (C) | kendi-assert harness (9 faz) | 4 araç 9/9 · cursor'da UB |
| 005 | QOI Codec | 🟡 | Bit-codec | referans `.qoi` ile byte-byte (encode+decode ayrı) | Opus'lar 16/16 · **codex & Gemini 8/16 (decoder bozuk)** |
| 002 | int2048 | 🟡 | Büyük sayı | 352 differential test (Python bigint = ground truth) | 4 araç 352/352 |
| 013 | STLite Map | 🟡 | Dengeli BST | 10 canonical test | 4 araç 10/10 · codex noexcept-bug, cursor null-deref |
| 006 | Minesweeper | 🔴 | Oyun/protokol | 40 test (referans server; win/lose/autoexplore) | 3 araç 40/40 · **antigravity DERLENMEDİ** |
| 014 | Python Interpreter | 🔴 | Yorumlayıcı (ANTLR4+C++) | 34 test, python3 ile kıyas | 3 araç 34/34 · **codex DERLENMEDİ** |
| 018 | Scheme Interpreter | 🔴 | Yorumlayıcı (R5RS, C++) | 118 test, **cross-agent consensus** (referans yok) | **4 araç 118/118** (hepsi uzlaştı) |
| 019 | GPU Attention Sim | 🔴 | Sistem/ML (matris) | 32 sorgu, resmi Rater + **gerçek ans.txt** (Error Rate) | **4 araç %100** |
| 004 | Bookstore Yönetim | 🔴 | Yönetim sistemi (standalone) | 60 test, **cross-agent consensus** (referans yok) | 3 araç 60/60 · **codex 55/60** · antigravity CCN 172 |
| 017 | Train Ticket Sistemi | 🔴 | EN BÜYÜK (disk-tabanlı, STL yasak) | 30 parça sıralı, **gerçek out.txt** (359k satır) | 3 araç %100 · **antigravity 698/359770 (%0.2) — her grupta segfault** (yeniden koşu, bkz. §4a) |

**Her problemin doğrulaması bir referans doğru-çözümle test edildi** (doğru kod tam puan alıyor mu, template düşük alıyor mu). 014 hariç (tam C++ interpreter yazılamadı → python3-sarmalayıcıyla + agent skorlarıyla ampirik doğrulandı; bkz. §5).

---

## 3. Ne nasıl ölçülüyor (metrikler · araç · yön)

> Yön: **↑** yüksek iyi · **↓** düşük iyi · **~** nötr. Her koşu için `analiz.py` (otomatik) bu metrikleri üretir.

| Metrik | Ölçüm aracı / mekanizma | Yön |
|--------|--------------------------|:---:|
| **dogruluk** (geçen/toplam) | problemin `oracle.py`'si: agent kodunu derleyip testlerle çalıştırır, çıktıyı beklenenle **satır-satır** kıyaslar | ↑ |
| **exec_score** (0–100) | doğruluk × 100 | ↑ |
| **guvenlik** | **cppcheck** (offline) — buffer overflow / null-deref / leak / uninitialized / noexcept-ihlali **bulgu sayısı** | ↓ |
| **uyari** | **g++ -Wall -Wextra** — agent dosyasına ait derleyici uyarısı sayısı | ↓ |
| **clang_tidy** | **clang-tidy** (119 check) — derin statik analiz uyarısı; derlenemezse "-" | ↓ |
| **leak_byte** | **valgrind** — "definitely lost" byte (framework'te küçük bir testte) | ↓ |
| **max_ccn** | **lizard** — en yüksek cyclomatic complexity (bakım zorluğu) | ↓ |
| **cr_score** (0–100) | proxy: 100'den başlar, complexity + cppcheck bulgusu cezası | ↑ |
| **yorum_orani** | yorum satırı / toplam (dokümantasyon) | ~ |
| **hiz_ms** | üretilen **kodun** duvar-saati süresi (agent'ın yazma süresi değil; disk G/Ç'si dahil, gürültülü) | ↓ |
| **cpu_ms** | **/usr/bin/time** — test başına en yüksek CPU süresi (user+sys); resmi zaman limiti buna uygulanır | ↓ |
| **max_mb** | /usr/bin/time — tepe bellek (RSS) | ↓ |
| **exec_lim** | ProjDevBench'in **resmi test-başı zaman/bellek limitleriyle** doğruluk (ajan yeniden koşturulmadan, sonradan uygulanır; limitler `calisma/olcum.py`) | ↑ |
| **tle / mle** | resmi limiti aşan test koşusu sayısı (zaman / bellek) | ↓ |
| **binary_kb** | derlenmiş dosya boyutu | ↓ |
| **commit / churn** | **git** — commit sayısı, eklenen/silinen satır (scaffold hariç) | ~ |
| **survival** | **git blame** — final kodun kaçı ilk commit'ten kaldı (%); düşük = çok yeniden yazma | ~ |
| **rewrite** | churn / satır (yeniden-yazma yoğunluğu) | ~ |
| **kendi_testi** | agent kendi test dosyasını yazdı mı | ↑ |
| **birlesik** (0–100) | `0.8 × exec_score + 0.2 × cr_score` (ProjDevBench formülü) — sadece ProjDevBench ile kalibrasyon için; araçlar boyut boyut karşılaştırılır | ↑ |
| model / mudahale / not | manuel (`kayit.csv`) | — |

**Kullanılan araçlar:** cppcheck (güvenlik) · clang-tidy (statik analiz) · valgrind (bellek sızıntısı) · lizard (complexity) · g++ (derleme/uyarı) · git (süreç). Hepsi offline, ücretsiz.

---

## 4. Kayıtlar nerede tutuluyor (danışman buraya baksın)

```
staj1/
├── tum-sonuclar.csv        ← ★ ANA VERİ: TÜM sonuçlar tek tabloda (12 problem × 4 araç = 48 satır)
├── kayit.csv               ← manuel notlar (model, müdahale, gözlem)
├── analiz.py               ← otomatik ölçüm scripti (her problemde kopyası var)
├── birlestir.py            ← problem sonuçlarını tum-sonuclar.csv'de birleştirir
├── NOTLAR-MANUEL.md        ← bu dosya (yöntem özeti)
│
├── calisma/problem-XXX/    ← her problem (002, 004, 005, 006, 009, 013, 014, 017, 018, 019, 020)
│   ├── sonuclar/<arac>/    ← ★ o aracın ürettiği KOD + .git geçmişi
│   ├── cfg.json            ← problemin build/kalite ayarı
│   ├── oracle.py           ← problemin doğrulama mantığı
│   ├── ref_data/ | ref/    ← canonical testler + beklenen çıktılar
│   ├── sonuclar.csv        ← o problemin 4 araç sonucu
│   └── rapor.txt           ← her testin girdi/beklenen/çıktı + tüm detay
│
└── pilot/problem-001/      ← pilot problem (A+B)
```

### 4a. Derleme her zaman kaynaktan (replication)

`analiz.py` her çözümü, çözüm klasörünün **git'te izlenen dosyalarından oluşturulan taze bir geçici kopyada** derler (`calisma/olcum.py: temiz_kopya`). Çözüm klasöründe kalmış `code` binary'leri, CMake cache'leri ve `build/` klasörleri **asla kullanılmaz**; derleme uyarıları, lizard, cppcheck ve clang-tidy de bu aynı temiz kopya üzerinde ölçülür. Consensus üreticileri (`consensus004.py`, `consensus018.py`) de kaynaktan derler.

- **Neden:** Eski sürümde `build()` klasörde hazır bir `code` bulursa onu test ediyordu. 017-antigravity'de kaynaktan daha eski bir ara binary test edilmiş ve 345/359770 (≈%0.1) raporlanmıştı; kaynaktan derlenince sonuç 359770/359770.
- **017-antigravity yeniden koşuldu (2026-09-29):** İlk koşudan klasörde kalan kod (commit 31df3b6, 17 Temmuz, "Co-authored-by: Cursor") Cursor'ın kendi 017 çözümünün (5a8138f) neredeyse birebir kopyasıydı: ~1500 satırın 10'u dışında aynı. 19 Temmuz'da da Cursor ile yeniden yazılmıştı (51b9966, 1185fd3). Antigravity'ye ait bağımsız bir kod bulunmadığı için koşu geçersiz sayıldı. Klasör iskelete (README.md + data/) döndürüldü ve Antigravity aynı promptla, Gemini 3.1 Pro (High) ile yeniden koşuldu. Yeni kod diğer araçlarla en fazla %7 benzer. Temiz derlemede her test grubunda segfault veriyor (698/359770); ajanın kendi Windows derlemesi de aynı girdide çöküyor. Eski içerik git geçmişinde duruyor.
- **Diğer 47 koşu:** Araç çiftleri arasındaki benzerlik taramasında kopya izi yok.
- **Ders:** Ajanlar yan klasörleri okuyabiliyor. Yeni koşularda her araç izole bir klasörde çalıştırılmalı.
- `olcum.temiz_kopya(sol_dir, commit)` gerekirse belirli bir commit'ten de derleyebilir (`cfg.json` → `"sabit_surum"`). Şu an hiçbir problemde kullanılmıyor.
- **Tekrar üretmek:** her problem klasöründe `python3 analiz.py` (WSL/Linux), ardından `python3 birlestir.py`; makale tabloları `python makale/tablolar.py`, şekil `python makale/make_figure.py`.

**Neye bakmak için nereye:**
- **Genel karşılaştırma / tüm skorlar** → `tum-sonuclar.csv`
- **Bir aracın bir problemdeki kodu** → `calisma/problem-XXX/sonuclar/<arac>/`
- **Bir ölçümün detayı (neden bu skor)** → `calisma/problem-XXX/rapor.txt`
- **Nasıl doğrulandığı (test mantığı)** → `calisma/problem-XXX/oracle.py`
- **Manuel gözlemler** → `kayit.csv`

---

## 5. Sonuç özeti

**12 problem ortalaması** (temiz derleme + resmi limitlerle yeniden skorlama sonrası):

| Araç | Model | Ort. E | Ort. E (resmi limit) | Ort. Birleşik | Özet |
|------|-------|:---:|:---:|:---:|------|
| Claude Code | Opus 4.8 | 100.0 | **100.0** | 98.7 | 12/12 doğru, tüm limitler içinde; 005 CCN + 004 uyarı |
| Cursor | Opus 4.8 | 100.0 | 91.7 | 99.1 | 12/12 doğru; **019'da bellek limiti aşıldı** (331 MiB > 244 MiB) |
| Codex | GPT-5.4 | 86.8 | 86.8 | 85.8 | 014'te derlenmedi, 005 decoder, 013 noexcept, 004'te 55/60 |
| Antigravity | Gemini 3.1 | 79.2 | 79.2 | 80.3 | 006'da derlenmedi (link hatası), **017'de segfault** (yeniden koşu), 005 decoder |

**Ana bulgular:**
1. İki **Opus 4.8** aracı doğrulukta önde — hiç build hatası yok. Resmi limitlerle tüm problemleri geçen tek araç Claude Code.
2. **005 QOI** en büyük ayırıcı: Opus'lar encode+decode tam; codex & Gemini 8/16.
3. **Tüm yapısal hatalar yüksek karmaşıklıkta:** 006 (antigravity), 014 (codex) derlenmedi; 017'de antigravity çöktü; 019'da cursor bellek limitini aştı. Codex ile Antigravity arasındaki fark tek bir ek yapısal hatadan (Antigravity'nin 017 çökmesi) geliyor.
4. **Doğruluk çoğu problemde eşitken** asıl ayrışma **güvenlik/kalite**de (cppcheck cursor'da UB+null-deref, codex'te noexcept-ihlali yakaladı).
5. **Complexity çok değişken:** aynı problemde CCN 46 (cursor, 018) ile 121 (antigravity, 018) arası — aynı doğruluk, çok farklı kod.

---

## 6. Kısıtlar (Threats to Validity — makalede belirtilecek)

1. **Doğruluk = kendi testlerimiz** → ACMOJ'un tam gizli seti kadar kapsamlı değil; bir bug kaçabilir.
2. **cr_score = lizard complexity proxy** → gerçek kod incelemesi değil (blind LLM review ayrıca planlanıyor).
3. **Güvenlik = cppcheck** (statik analiz) → bazı mantık açıklarını kaçırabilir. (Not: semgrep C/C++'ta offline çalışmadığı için cppcheck'e geçildi.)
4. **hız/bellek = yerel makine** → standart OJ donanımı değil (ama tüm araçlar aynı makinede → göreceli adil). Resmi limitler en gevşek (max) değerle, CPU süresi ve tepe RSS üzerinden sonradan uygulanır → ihlal sayıları alt sınırdır; testlerimiz gizli setlerden küçük olduğu için büyük girdide çıkacak TLE'ler yakalanmaz. Agent **çalışma süresi ölçülmüyor** (onay bekleme + internet gecikmesi kirletiyor).
5. **014 oracle = python3** → simplified-Python'dan ayrışan 2 test (scoping, f-string float) hariç tutuldu; kalan 34 test python3 ile birebir, agent skorlarıyla ampirik doğrulandı.
6. **018 oracle = cross-agent consensus** (referans yok — score/test sahte, simplified-Scheme özel semantik). 4 bağımsız yorumlayıcının çoğunluk uzlaşması "doğru" kabul edildi; 118 testin **hepsinde 4 araç uzlaştı** (0 ambiguous) → çok güvenilir ama teorik olarak %100 kesin değil (tüm araçlar aynı yanlışı yaparsa yakalanmaz).
7. **N=1** → tek koşu (stokastik); tekrar (N≥3) opsiyonel gelecek iş.
8. **Contamination** → problemler public (SJTU); araçlar ezberlemiş olabilir.

→ **Sonuç:** Mutlak "doğru kalite" ölçmüyor ama **araçlar arası göreceli kıyas için geçerli** — hepsi aynı terazide, aynı ortamda, aynı promptla tartıldı.

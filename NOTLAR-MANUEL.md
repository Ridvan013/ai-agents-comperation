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
| 017 | Train Ticket Sistemi | 🔴 | EN BÜYÜK (disk-tabanlı, STL yasak) | 30 parça sıralı, **gerçek out.txt** (359k satır) | 3 araç %100 · **antigravity ~%0.1 (çöktü+TLE)** |

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
| **hiz_ms** | **/usr/bin/time** — üretilen **kodun** çalışma hızı (agent'ın yazma süresi değil) | ↓ |
| **max_mb** | /usr/bin/time — tepe bellek (RSS) | ↓ |
| **binary_kb** | derlenmiş dosya boyutu | ↓ |
| **commit / churn** | **git** — commit sayısı, eklenen/silinen satır (scaffold hariç) | ~ |
| **survival** | **git blame** — final kodun kaçı ilk commit'ten kaldı (%); düşük = çok yeniden yazma | ~ |
| **rewrite** | churn / satır (yeniden-yazma yoğunluğu) | ~ |
| **kendi_testi** | agent kendi test dosyasını yazdı mı | ↑ |
| **birlesik** (0–100) | **ANA SKOR** = `0.8 × exec_score + 0.2 × cr_score` (ProjDevBench formülü) | ↑ |
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

**Neye bakmak için nereye:**
- **Genel karşılaştırma / tüm skorlar** → `tum-sonuclar.csv`
- **Bir aracın bir problemdeki kodu** → `calisma/problem-XXX/sonuclar/<arac>/`
- **Bir ölçümün detayı (neden bu skor)** → `calisma/problem-XXX/rapor.txt`
- **Nasıl doğrulandığı (test mantığı)** → `calisma/problem-XXX/oracle.py`
- **Manuel gözlemler** → `kayit.csv`

---

## 5. Sonuç özeti

**12 problem ortalaması (birleşik skor):**

| Sıra | Araç | Model | Ort. Birleşik | Özet |
|:---:|------|-------|:---:|------|
| 🥇 | Cursor | Opus 4.8 | **99.1** | 12/12 doğru, ufak güvenlik dingleri; en temiz kod |
| 🥈 | Claude Code | Opus 4.8 | **98.7** | 12/12 doğru, hiç çökmedi; 005 CCN + 004 uyarı |
| 🥉 | Codex | GPT-5.4 | **85.8** | 014'te derlenmedi, 005 decoder, 013 noexcept, 004'te 55/60 |
| 4 | Antigravity | Gemini 3.1 | **80.3** | 006'da derlenmedi, **017'de çöktü (~%0.1+TLE)**, 005 decoder |

**Ana bulgular:**
1. İki **Opus 4.8** aracı belirgin önde — hiç build hatası yok, hep doğru.
2. **005 QOI** en büyük ayırıcı: Opus'lar encode+decode tam; codex & Gemini decoder'da çöktü.
3. **En zor problemlerde ayrışma keskinleşiyor:** 006 (antigravity), 014 (codex) derlenmedi; **017 (en büyük — Train Ticket, STL yasak + disk kalıcılık) antigravity'yi çökertti** (~%0.1 + TLE) ama Opus'lar + codex %100. 018/019'da 4 araç da başarılı → tek "zor problem şampiyonu" yok, ama **iki Opus aracı her zaman ayakta**.
4. **Doğruluk çoğu problemde eşitken** asıl ayrışma **güvenlik/kalite**de (cppcheck cursor'da UB+null-deref, codex'te noexcept-ihlali yakaladı).
5. **Complexity çok değişken:** aynı problemde CCN 46 (cursor, 018) ile 121 (antigravity, 018) arası — aynı doğruluk, çok farklı kod.

---

## 6. Kısıtlar (Threats to Validity — makalede belirtilecek)

1. **Doğruluk = kendi testlerimiz** → ACMOJ'un tam gizli seti kadar kapsamlı değil; bir bug kaçabilir.
2. **cr_score = lizard complexity proxy** → gerçek kod incelemesi değil (blind LLM review ayrıca planlanıyor).
3. **Güvenlik = cppcheck** (statik analiz) → bazı mantık açıklarını kaçırabilir. (Not: semgrep C/C++'ta offline çalışmadığı için cppcheck'e geçildi.)
4. **hız/bellek = yerel makine** → standart OJ donanımı değil (ama tüm araçlar aynı makinede → göreceli adil). Agent **çalışma süresi ölçülmüyor** (onay bekleme + internet gecikmesi kirletiyor).
5. **014 oracle = python3** → simplified-Python'dan ayrışan 2 test (scoping, f-string float) hariç tutuldu; kalan 34 test python3 ile birebir, agent skorlarıyla ampirik doğrulandı.
6. **018 oracle = cross-agent consensus** (referans yok — score/test sahte, simplified-Scheme özel semantik). 4 bağımsız yorumlayıcının çoğunluk uzlaşması "doğru" kabul edildi; 118 testin **hepsinde 4 araç uzlaştı** (0 ambiguous) → çok güvenilir ama teorik olarak %100 kesin değil (tüm araçlar aynı yanlışı yaparsa yakalanmaz).
7. **N=1** → tek koşu (stokastik); tekrar (N≥3) opsiyonel gelecek iş.
8. **Contamination** → problemler public (SJTU); araçlar ezberlemiş olabilir.

→ **Sonuç:** Mutlak "doğru kalite" ölçmüyor ama **araçlar arası göreceli kıyas için geçerli** — hepsi aynı terazide, aynı ortamda, aynı promptla tartıldı.

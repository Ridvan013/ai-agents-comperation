# AI Kod Araçları Karşılaştırması — Proje Özeti

**Amaç:** 4 AI kod aracının (Claude Code · Cursor · Codex · Antigravity) **aynı programlama problemlerini aynı prompt'la** çözdürülüp; ürettikleri kodun **doğruluk · kalite · güvenlik · verimlilik · süreç** boyutlarında karşılaştırılması.

**Yöntem:** Manuel yürütme (abonelik/ücretsiz erişim, ~$0 maliyet). Doğruluk, SJTU/ACMOJ online judge'a erişim olmadığı için **kendi yerel checker'larımız + kendi gizli testlerimizle** ölçülür. Temel: ProjDevBench (arXiv 2602.01655). Tüm ölçümler WSL/Linux'ta, **tüm araçlar aynı ortamda** (göreceli kıyas adil).

**Durum:** ✅ **8/8 problem tamam** — 32 koşu (8 problem × 4 araç). Sıradaki: istatistik + makale.

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
├── tum-sonuclar.csv        ← ★ ANA VERİ: TÜM sonuçlar tek tabloda (8 problem × 4 araç = 32 satır)
├── kayit.csv               ← manuel notlar (model, müdahale, gözlem)
├── analiz.py               ← otomatik ölçüm scripti (her problemde kopyası var)
├── birlestir.py            ← problem sonuçlarını tum-sonuclar.csv'de birleştirir
├── NOTLAR-MANUEL.md        ← bu dosya (yöntem özeti)
│
├── calisma/problem-XXX/    ← her problem (002, 005, 006, 009, 013, 014, 020)
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

**8 problem ortalaması (birleşik skor):**

| Sıra | Araç | Model | Ort. Birleşik | Özet |
|:---:|------|-------|:---:|------|
| 🥇 | Claude Code | Opus 4.8 | ~99 | 8/8 doğru, hiç çökmedi |
| 🥈 | Cursor | Opus 4.8 | ~99 | 8/8 doğru, ufak güvenlik dingleri (UB, null-deref) |
| 🥉 | Antigravity | Gemini 3.1 | ~82 | 006'da derlenmedi, 005 decoder zayıf |
| 4 | Codex | GPT-5.4 | ~81 | 014'te derlenmedi, 005 decoder + 013 noexcept |

**Ana bulgular:**
1. İki **Opus 4.8** aracı belirgin önde — 8 problemde hiç build hatası yok, hep doğru.
2. **005 QOI** en büyük ayırıcı: Opus'lar encode+decode tam; codex & Gemini decoder'da çöktü.
3. **En zor 2 problemde farklı yerlerde tökezlediler:** codex 014'te, antigravity 006'da derlenmedi → "zor problem şampiyonu" yok.
4. **Doğruluk çoğu problemde eşitken** asıl ayrışma **güvenlik/kalite**de (cppcheck cursor'da UB+null-deref, codex'te noexcept-ihlali yakaladı).

---

## 6. Kısıtlar (Threats to Validity — makalede belirtilecek)

1. **Doğruluk = kendi testlerimiz** → ACMOJ'un tam gizli seti kadar kapsamlı değil; bir bug kaçabilir.
2. **cr_score = lizard complexity proxy** → gerçek kod incelemesi değil (blind LLM review ayrıca planlanıyor).
3. **Güvenlik = cppcheck** (statik analiz) → bazı mantık açıklarını kaçırabilir. (Not: semgrep C/C++'ta offline çalışmadığı için cppcheck'e geçildi.)
4. **hız/bellek = yerel makine** → standart OJ donanımı değil (ama tüm araçlar aynı makinede → göreceli adil). Agent **çalışma süresi ölçülmüyor** (onay bekleme + internet gecikmesi kirletiyor).
5. **014 oracle = python3** → simplified-Python'dan ayrışan 2 test (scoping, f-string float) hariç tutuldu; kalan 34 test python3 ile birebir, agent skorlarıyla ampirik doğrulandı.
6. **N=1** → tek koşu (stokastik); tekrar (N≥3) opsiyonel gelecek iş.
7. **Contamination** → problemler public (SJTU); araçlar ezberlemiş olabilir.

→ **Sonuç:** Mutlak "doğru kalite" ölçmüyor ama **araçlar arası göreceli kıyas için geçerli** — hepsi aynı terazide, aynı ortamda, aynı promptla tartıldı.

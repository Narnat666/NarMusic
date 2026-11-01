# 外部接口实测报告（B 站 + 四家歌词平台）

- 实测时间：2026-11-01（UTC+8 上午，响应头 `Date: Sun, 01 Nov 2026`）
- 实测环境：Windows，`python 3.14.7`、`curl.exe`（C:\Windows\System32\curl.exe）、`ffprobe.exe`（E:\Scoop\shims\ffprobe.exe）、`ffmpeg.exe`（E:\Scoop\shims\ffmpeg.exe）；直连外网、无系统代理
- 所有脚本与原始响应都在 `E:\桌面\安卓软件\NarMusicAndroid\docs\probe\`，本文件中的每个命令都可在该目录直接复跑
- 临时样本总量：`docs\probe\` 全目录 9,504,933 字节（其中两个 `.m4a` 共 8,371,125 字节，远低于 60MB 上限）

复跑前先切目录：

```bat
cd /d "E:\桌面\安卓软件\NarMusicAndroid\docs\probe"
```

## 0. 九组结论一览

| # | 接口 | 结论 |
|---|---|---|
| 1 | B 站 `x/web-interface/view` | **可用** |
| 2 | B 站 `x/player/playurl`（dash 音频直链） | **可用**（moov 前置，下载完整即合法 M4A） |
| 3 | B 站 WBI 搜索 | **可用**（匿名无 Cookie 也可用，`nav` 未登录仍给 wbi_img） |
| 4 | 酷狗歌词（search/download lrc+krc） | **可用**（`json.trans` 字段已不存在，翻译改在 KRC 的 `[language:]` 段） |
| 5 | 网易云歌词 | **可用**（`cloudsearch/pc` + `song/lyric`，无需登录；`search/get` 亦可用） |
| 6 | QQ 音乐歌词 | **可用**（POST 与 GET 降级都 200；`req.data.body.song.list` 结构不变；`lyric`/`trans` 为 base64） |
| 7 | 汽水音乐 | `api.qishui.com` 两个 API **不可用**（200 + 空体）；`music.douyin.com` 分享页**可用**（需 track_id） |
| 8 | B 站音频完整下载 + ffprobe | **可用**，ffprobe 判定为合法 m4a（`probe_score=100`） |
| 9 | `NarMusic\download\` 历史产物 | **不存在**（目录本身不存在，见第 9 节） |

---

## 0.1 复跑脚本清单（全在 `docs\probe\`）

| 脚本 | 作用 |
|---|---|
| `bili_wbi.py 周杰伦` | spi 取匿名 cookie → nav 取 wbi key → 签名搜索，打印完整签名 URL |
| `bili_nocookie.py` | 完全不带 Cookie 的 nav + 搜索对照实验 |
| `bili_findshort.py` | 批量搜索并按时长筛出 1~3 分钟的视频（用于选第 8 组样本） |
| `bili_audio_probe.py` | playurl 取 baseUrl/backupUrl → 取前 64KB → 顶层 box + moov 子树 |
| `mp4boxes.py <file> [depth]` | 通用 MP4 box 树打印 |
| `ilst_dump.py <file>` | 打印 `moov.udta.meta.ilst` 下所有标签原子与内容 |
| `bili_curl_head64k.py` | 生成并执行 `run_curl_head64k.bat`（curl 取前 64KB + 保存响应头） |
| `bili_init_probe.py <aid> <cid>` | init 段 Range 试探 + `fnval` 0/1/16/4048 对照 |
| `bili_dl_headers.py` | 下载头（UA/Referer）缺失时的 403 验证 |
| `bili_download_full.py` | 完整下载一首 1~3 分钟音频，打印字节数与耗时 |
| `kugou_probe.py "<歌手 - 歌名>"` | 酷狗搜索 → 歌词候选 → lrc/krc 下载 → base64 → KRC 解密 |
| `kugou_language.py` | 解析 KRC 里 `[language:]` 段的翻译 JSON |
| `netease_probe.py "<关键词>"` | 网易云 cloudsearch / search-get / lyric 三条链路 |
| `netease_lyric_by_id.py <id>...` | 按 id 直接验证 `lrc.lyric` / `tlyric.lyric` |
| `qq_make_body.py "<关键词>" [mid]` | 生成 `qq_search_body.json` / `qq_lyric_body.json` |
| `qq_show_lyric.py <json>` | 校验 `lyric`/`trans` 的 base64 并解出前几行 |
| `qq_get_fallback.py` | QQ 的 GET 降级实测 |
| `qishui_probe.py` | 汽水搜索接口 3 种变异 |
| `qishui_probe2.py` | `track_v2` + 搜索复测 + 分享页 |
| `qishui_shortlink.py` | 短链 → `track_id` → 分享页 |
| `qishui_parse_share.py` | 解析分享页 `audioWithLyricsOption` 与 `lyrics.sentences` |
| `header_variants.py` | 网易云去头变异 + QQ 搜索 GET 降级 |
| `make_tagged_sample.py` | ffmpeg 往样本里写 `©nam/©lyr/covr` 并打印 ilst（第 9 组替代证据） |

关键原始响应文件：`bili_view.json`、`bili_playurl.json`、`bili_search_raw.json`、`bili_nav.json`、`bili_spi.json`、`kugou_search.json`、`kugou_lyric_search.json`、`kugou_dl_lrc.json`、`kugou_dl_krc.json`、`kugou_lyric.lrc`、`kugou_lyric_decrypted.krc`、`netease_cloudsearch.json`、`netease_lyric.json`、`netease_lyric_185809.json`、`qq_search.json`、`qq_lyric.json`、`qq_lyric_en.json`、`qq_lyric_get.json`、`qishui_search_resp.txt`、`qishui_share_real.html`、`qishui_lyric_awo.json`、`ffprobe_out.txt`、`bili_audio_urls.txt`。

---

## 1. B 站视频信息（`x/web-interface/view`）—— 可用

样本 bvid 来自第 3 组 WBI 搜索（关键词「周杰伦 稻香」）。

```bat
curl.exe -s -o bili_view.json -w "HTTP=%{http_code}\n" ^
  -H "Referer: https://www.bilibili.com" -H "Origin: https://www.bilibili.com" ^
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36" ^
  "https://api.bilibili.com/x/web-interface/view?bvid=BV1iuTj6dEtJ"
python -c "import json,io;d=json.load(io.open('bili_view.json',encoding='utf-8'));print('code=',d['code'],'msg=',d['message']);k=d['data'];print('aid=',k['aid']);print('cid=',k['cid']);print('title=',k['title']);print('duration=',k['duration']);print('pages=',[(p['cid'],p['part'],p['duration']) for p in k['pages']])"
```

**HTTP 状态码：200**

响应（`python` 摘要，字段直取 `data.*`）：

```
HTTP=200
code= 0 msg= OK
aid= 116849853337990
cid= 39590363616
title= 这才是《稻香》原版MV！
duration= 201
owner= 妙手回春杨某人
pages= [(39590363616, '这才是《稻香》原版MV！', 201)]
```

结论：**可用**，字段名与 `bilibili_client.cpp::fetchVideoInfo` 完全一致（`data.aid`、`data.cid`、`data.title`）。注意 `aid` 已达 15 位（116849853337990），Kotlin 侧必须用 `Long`（而非 `Int`）；`cid` 也可能超过 `Int`。多 P 视频取 `data.pages[0].cid` 与原 `data.cid` 相同。

---

## 2. B 站音频直链（`x/player/playurl`）+ MP4 box 结构 —— 可用

### 2.1 取直链

```bat
curl.exe -s -o bili_playurl.json -w "HTTP=%{http_code}\n" ^
  -H "Referer: https://www.bilibili.com" -H "Origin: https://www.bilibili.com" ^
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36" ^
  "https://api.bilibili.com/x/player/playurl?avid=116849853337990&cid=39590363616&qn=0&type=&otype=json&fnver=0&fnval=80"
python bili_audio_probe.py
```

**HTTP 状态码：200**

响应摘要：

```
code= 0 msg= OK
data.keys= ['from','result','message','quality','format','timelength','accept_format','accept_description','accept_quality','video_codecid','seek_param','seek_type','dash','support_formats','high_format','last_play_time','last_play_cid','view_info','play_conf','cur_language','cur_production_type','auto_qn_resp']
audio streams: 3
  id=30216 bandwidth=65709  codecs=mp4a.40.2 mime=audio/mp4 backupUrl=2
  id=30232 bandwidth=97206  codecs=mp4a.40.2 mime=audio/mp4 backupUrl=2
  id=30280 bandwidth=201770 codecs=mp4a.40.2 mime=audio/mp4 backupUrl=2
```

`data.dash.audio[2]`（192k 档）完整 `baseUrl`（2026-11-01 上午生成，`deadline=1793511981`，约 2 小时后失效）：

```
https://b-baaa1gvj119u13vbgs9iegnauj0c.edge.mountaintoys.cn:4483/upgcxcode/16/36/39590363616/39590363616-1-30280.m4s?e=ig8euxZM2rNcNbdlhoNvNC8BqJIzNbfqXBvEqxTEto8BTrNvN0GvT90W5JZMkX_YN0MvXg8gNEV4NC8xNEV4N03eN0B5tZlqNxTEto8BTrNvNeZVuJ10Kj_g2UB02J0mN0B5tZlqNCNEto8BTrNvNC7MTX502C8f2jmMQJ6mqF2fka1mqx6gqj0eN0B599M=&uipk=5&oi=0x240e036f15c254a129d03c597b2a54ec&trid=00005d5996157f1d4eada3f8df30d590156u&deadline=1793511981&platform=pc&os=mcdn&mid=0&nbs=1&gen=playurlv3&og=ali&upsig=6caa89f355f79b60bd6e16e888307957&uparams=e,uipk,oi,trid,deadline,platform,os,mid,nbs,gen,og&mcdnid=50060845&sche=rspull&u=0.35&bvc=vod&nettype=0&bw=202631&lrs=39&V=v3.0.7&prs=2&build=0&dl=0&f=u_0_0&qn_dyeid=14fa420689fb50f6001f159e6ae6b60d&agrr=0&buvid=&orderid=0,3
```

`backupUrl[0]`（`backupUrl` 共 2 条，`base_url`/`baseUrl` 同时存在）：

```
https://upos-sz-mirrorcoso1.bilivideo.com/upgcxcode/16/36/39590363616/39590363616-1-30280.m4s?e=ig8euxZM2rNcNbdlhoNvNC8BqJIzNbfqXBvEqxTEto8BTrNvN0GvT90W5JZMkX_YN0MvXg8gNEV4NC8xNEV4N03eN0B5tZlqNxTEto8BTrNvNeZVuJ10Kj_g2UB02J0mN0B5tZlqNCNEto8BTrNvNC7MTX502C8f2jmMQJ6mqF2fka1mqx6gqj0eN0B599M=&platform=pc&nbs=1&uipk=5&oi=0x240e036f15c254a129d03c597b2a54ec&deadline=1793511981&os=coso1bv&og=cos&mid=0&trid=5d5996157f1d4eada3f8df30d590156u&gen=playurlv3&upsig=a95fbafd7a7644926909b305ad8dfc3d&uparams=e,platform,nbs,uipk,oi,deadline,os,og,mid,trid,gen&bvc=vod&nettype=0&bw=202631&lrs=39&V=v3.0.7&qn_dyeid=14fa420689fb50f6001f159e6ae6b60d&agrr=0&buvid=&build=0&dl=0&f=u_0_0&orderid=1,3
```

### 2.2 用 curl 只取前 64KB 并解析 box

```bat
python bili_curl_head64k.py      :: 重新取 playurl 拿最新 baseUrl，生成并执行 run_curl_head64k.bat
type bili_head64k_headers.txt
python mp4boxes.py bili_audio_head64k_curl.bin 1
```

`run_curl_head64k.bat` 的实质命令（baseUrl 见 2.1）：

```bat
curl.exe -s -r 0-65535 -o bili_audio_head64k_curl.bin -D bili_head64k_headers.txt ^
  -H "Referer: https://www.bilibili.com" ^
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36" ^
  "<baseUrl>"
```

响应头原文（`HTTP/1.1 206 Partial Content`）：

```
HTTP/1.1 206 Partial Content
Last-Modified: Thu, 02 Jul 2026 10:17:39 GMT
Etag: "b794488903e22e959345c7725f4d60ce"
Content-Range: bytes 0-65535/5065782
Content-Length: 65536
Server: lego_acgvideo
Date: Sun, 01 Nov 2026 03:52:56 GMT
X-Cache-Lookup: Cache Hit
X-Upsig-Version: 20220801
Content-Type: application/octet-stream
```

box 序列（原始打印，只展开 1 层）：

```
file = ...\docs\probe\bili_audio_head64k_curl.bin  size = 65536 bytes
ftyp   size=32       offset=0
moov   size=805      offset=32
  mvhd   size=108      offset=40
  trak   size=480      offset=148
  mvex   size=56       offset=628
  udta   size=153      offset=684
sidx   size=532      offset=837
moof   size=1040     offset=1369
  mfhd   size=16       offset=1377
  traf   size=1016     offset=1393
mdat   size=113496   offset=2409
```

（`python bili_audio_probe.py` 用 urllib 走了一遍同样的流程，结果完全一致；该脚本额外打印完整 `moov` 子树与 ilst。）

`moov` 子 box（`python mp4boxes.py bili_audio_head64k.bin`）：

```
ftyp   size=32       offset=0
moov   size=805      offset=32
  mvhd   size=108      offset=40
  trak   size=480      offset=148
    tkhd   size=92       offset=156
    edts   size=36       offset=248
      elst   size=28       offset=256
    mdia   size=344      offset=284
      mdhd / hdlr / minf(smhd, dinf(dref), stbl(stsd,stts,stsc,stsz,stco))
  mvex   size=56       offset=628
    mehd   size=16       offset=636
    trex   size=32       offset=652
  udta   size=153      offset=684
    meta   size=145      offset=692
      hdlr   size=33       offset=704
      ilst   size=100      offset=737
sidx   size=532      offset=837
moof   size=1040     offset=1369
  mfhd size=16 / traf(tfhd, tfdt, trun)
mdat   size=113496   offset=2409
```

`moov.udta.meta.ilst` 里的标签原子（`python ilst_dump.py bili_audio_head64k.bin`）：

```
ilst: (737, 100) (meta 为 fullbox，子 box 从 +12 开始)

ilst 内原子：
  atom='©too'   size=36   offset=745
       data type=1 len=12 value='Lavf61.7.105'
  atom='desc'   size=56   offset=781
       data type=1 len=32 value='Packed by Bilibili XCoder v2.0.2'
```

### 2.3 init 段试探（`python bili_init_probe.py 116849853337990 39590363616`）

```
Range=bytes=0-836    HTTP=206 Content-Range=bytes 0-836/5065782   字节=837   boxes=[('ftyp', 32), ('moov', 805)]
Range=bytes=0-837    HTTP=206 Content-Range=bytes 0-837/5065782   字节=838   boxes=[('ftyp', 32), ('moov', 805)]
Range=bytes=0-1023   HTTP=206 Content-Range=bytes 0-1023/5065782  字节=1024  boxes=[('ftyp', 32), ('moov', 805), ('sidx', 532)]
```

同一脚本对 `fnval` 的其它取值：

```
  fnval=0     code=0 data.keys=['durl']  durl[0].size=12753255 length=200832  (完整 .mp4，含视频流)
  fnval=1     code=0 data.keys=['durl']
  fnval=16    code=0 data.keys=['dash']  dash.audio ids=[30216, 30232, 30280]
  fnval=4048  code=0 data.keys=['dash']  dash.audio ids=[30216, 30280, 30232]
```

### 2.4 结论

- 下载到的数据**是自包含可播放的 M4A，含 `moov`**。
- `moov` 紧跟在 `ftyp` 之后（第 2 个 box，offset 32），**在 `sidx`/`moof`/`mdat` 之前**；完整序列 `ftyp → moov → sidx → (moof → mdat)×36`。虽然带 `mvex`（fragmented 标志），但 init 段在文件头，**整段字节流直接存成 `.m4a` 就能播**（第 8 组 ffprobe 独立验证）。
- init 段 = 文件前 837 字节（`ftyp` 32 + `moov` 805），用 `Range: bytes=0-836` 一次拿到（HTTP 206）。若将来某个流的 `moov` 被后置，可先用 range 二分定位，或改用 `fnval=0/1` 走 `durl`（完整 mp4，但含视频流、体积更大）。
- `moov.udta.meta.ilst` **存在**，但只有 `©too`（`Lavf61.7.105`）和 `desc`（`Packed by Bilibili XCoder v2.0.2`）；**没有 `©nam`/`©lyr`/`covr`/`----`**。即 B 站原始流本身不含标题/歌词/封面标签，标签必须由客户端写入。
- 下载**必须同时带 `User-Agent` 和 `Referer`**，缺任一个都 403（`python bili_dl_headers.py`，同一 baseUrl、`Range: bytes=0-1023`）：

```
同时带 UA + Referer       HTTP=206 bytes=1024
只带 UA，无 Referer        HTTP=403 bytes=552 body前120=b'<html>\r\n<head><title>403 Forbidden</title></head>\r\n<body>\r\n<center><h1>403 Forbidden</h1></center>\r\n<hr><center>openrest'
只带 Referer，无 UA        HTTP=403 bytes=150 ...
都不带                    HTTP=403 bytes=150 ...
```

- 响应 `Content-Type` 是 `video/mp4`（即使只有音频），不要按 `audio/*` 过滤；同一 URL 换 CDN 也可能返回 `application/octet-stream`。

---

## 3. B 站 WBI 搜索 —— 可用（匿名可搜）

```bat
python bili_wbi.py 周杰伦        :: 取 spi cookie → nav 取 wbi keys → 签名搜索
python bili_nocookie.py          :: 完全不带 Cookie 的对照实验
```

**第 1 步 `x/frontend/finger/spi`（HTTP 200）**：

```
[spi] HTTP 200
[spi] cookie = buvid3=A4500911-B66A-6D22-D973-968249BB69F764566infoc; buvid4=89D0876A-9736-5AE3-8187-1C913C3C63D464566-026110111-Fefl2DIX5iMp4LzYhS4NTQ==
```

**第 2 步 `x/web-interface/nav`（HTTP 200）**：

```
[nav] HTTP 200
[nav] code = -101 message = 账号未登录
[nav] wbi_img.img_url = https://i0.hdslb.com/bfs/wbi/7cd084941338484aae1ad9425b84077c.png
[nav] wbi_img.sub_url = https://i0.hdslb.com/bfs/wbi/4932caff0ff746eab6f01bf08b70ac45.png
[nav] img_key = 7cd084941338484aae1ad9425b84077c  sub_key = 4932caff0ff746eab6f01bf08b70ac45
[wbi] mixin_key = ea1db124af3c7062474693fa704f4ff8
```

**第 3 步 签名后实际请求的完整 URL（含 `wts`）**：

```
https://api.bilibili.com/x/web-interface/wbi/search/all/v2?keyword=%E5%91%A8%E6%9D%B0%E4%BC%A6&page=1&pagesize=20&wts=1793504764&w_rid=9f14f19763cb5b0ace54872de7ba1aef
```

**HTTP 200，`code = 0`**：

```
{"code":0,"message":"OK","ttl":1,"data":{"seid":"5209318555266249090","page":1,"pagesize":20,...
[search] code = 0  message = OK
[search] result groups = 12  video groups = 1
    BV1q4FTeBEMj 419:2 58602211 | 【无损音质】<em class="keyword">周杰伦</em>歌曲精选10
    BV1FPjy6TEiE 222:28 19136999 | 【<em class="keyword">周杰伦</em>】50首精选合集/后台
    BV1VoxF6sE41 425:5 18632 | 【<em class="keyword">周杰伦</em>】最热门100首热歌、
```

`result_type=="video"` 的条目原文字段：

```json
{
 "type": "video",
 "id": 113911642789603,
 "bvid": "BV1q4FTeBEMj",
 "title": "【无损音质】<em class=\"keyword\">周杰伦</em>歌曲精选100首 歌词版纯享 地表最高音质 每一首都是顶级文案 中国流行音乐开拓者JAY",
 "duration": "419:2",
 "play": 58602211,
 "author": "无损音乐合集",
 "pubdate": 1738326240
}
```

`result[]` 的 `result_type` 出现顺序：`tips, brand_ad, esports, activity, web_game, card, media_bangumi, media_ft, bili_user, user, star, video` —— **`video` 是最后一组，必须遍历全部 12 组再筛 `result_type=="video"`**。

**匿名可用性对照（`python bili_nocookie.py`，HTTP 200）**：

```
=== A) 完全不带 Cookie 的 nav ===
HTTP 200 code = -101 message = 账号未登录
wbi_img 存在 = True  img_url = https://i0.hdslb.com/bfs/wbi/7cd084941338484aae1ad9425b84077c.png
mixin_key = ea1db124af3c7062474693fa704f4ff8

=== B) 完全不带 Cookie 的 wbi 搜索 ===
url = https://api.bilibili.com/x/web-interface/wbi/search/all/v2?keyword=%E5%91%A8%E6%9D%B0%E4%BC%A6&page=1&pagesize=20&wts=1793504772&w_rid=69b391ae220f15073808316c0e4b6950
HTTP 200 code = 0 message = OK
video groups = 1 ; 首条 = ('BV1q4FTeBEMj', '419:2')
```

结论：**可用**。
- 匿名（完全不带 Cookie）搜索成功：`code==0`，`w_rid` 只需 `mixin_key`，不需要登录态。
- `nav` 在未登录时仍返回 `data.wbi_img`（`code=-101` 只表示未登录，**不要用 `code` 判断 wbi 密钥是否可用**，要看 `data.wbi_img` 是否存在）。
- 密钥 `7cd0849…` / `4932caf…` 与 `bilibili_client.cpp` 里硬编码的 fallback 常量**当前完全相同**，所以即使 nav 失败也能签出有效 `w_rid`（但这是巧合，B 站轮换后会失效）。
- **`duration` 是字符串 `"419:2"`（分:秒，秒不补零）**，不是秒数、也不是整数秒；`"3:43"` 这类 2 段式把 `parts[0]*60+parts[1]` 才能得到 223 秒。原项目的多分支解析逻辑必须保留。
- `title` 含 `<em class="keyword">` HTML 标签，需 strip。

---

## 4. 酷狗歌词 —— 可用

```bat
python kugou_probe.py "周杰伦 - 稻香"
python kugou_language.py            :: 解析 KRC 内 [language:] 的翻译段
```

**第 1 步搜索（HTTP 200）**：

```
GET http://mobilecdn.kugou.com/api/v3/search/song?format=json&keyword=%E5%91%A8%E6%9D%B0%E4%BC%A6%20-%20%E7%A8%BB%E9%A6%99&page=1&pagesize=10&showtype=1
HTTP 200 body bytes 22873
status = 1  errmsg = None  total = 425
   hash=8909e1809908cd8e3bf6cf85d98b93f0 | 周杰伦 - 稻香 | album_id=960399 | dur=223
```

**第 2 步 `lyrics.kugou.com/search`（HTTP 200，带 `Cookie: kg_mid=<32位hex>` 与 `Referer: http://www.kugou.com`）**：

```
HTTP 200 body bytes 10125
status = 200  errcode = 200  candidates = 19
candidate: {"id": "165166985", "product_from": "ugc", "accesskey": "DF743843795684181ADE03FF627F0646", ... "duration": 223609, "krctype": 1, ...}
```

**第 3 步 `lyrics.kugou.com/download&fmt=lrc`（HTTP 200）**：

```
HTTP 200 body bytes 3044
keys = ['status', 'info', 'error_code', 'fmt', 'contenttype', '_source', 'charset', 'content', 'id']  status = 200  fmt = lrc  charset = utf8
has trans field = False
content base64 len = 2912 -> decoded bytes = 2184
LRC 前 5 行:
    ﻿[id:$00000000]

    [ar:周杰伦]

    [ti:稻香]
```

（原始 LRC 完整文件：`docs/probe/kugou_lyric.lrc`，126 行，标签行之间有空行。）

**第 4 步 `&fmt=krc`（HTTP 200）**：

```
HTTP 200 body bytes 4272
keys = ['status', 'info', 'error_code', 'fmt', 'contenttype', '_source', 'charset', 'content', 'id']  status = 200  fmt = krc  charset =
has trans field = False
content base64 len = 4144 -> decoded bytes = 3106
KRC magic = b'krc1'
KRC 解密成功: XOR+inflate 后 7704 字节
```

KRC 的 `[language:]` 段（日文歌 `米津玄師 - Lemon` 样本，`python kugou_language.py`）：

```
language 段 base64 长度 = 8664  解码 JSON keys = ['content', 'version']
content 条数 = 2
  [0] language=0 行数=57 第2行=' '
  [1] language=0 行数=57 第2行='sa ku shi : yo ne tsu ge n shi '
KRC 时间戳数量 = 57  前 5 个 = ['0', '366', '883', '1134', '2380']
```

结论：**可用**，四步全部 HTTP 200、无需登录。
- **`json.trans` 字段已不存在**（稻香 / Lemon / Adele-Hello 三个样本的 `download` 响应都不含 `trans`）。翻译改为内嵌在 **KRC 的 `[language:<base64>]`** 段里（JSON `{"content":[{"lyricContent":[[...]]},...]}`，与 `kugou_provider.cpp::decryptKRC` 的解析假设一致）。
- KRC 格式确认仍是 `krc1` 魔数 + 16 字节固定 XOR key + **zlib（inflate）**，python 一次解密成功；解压后长度为原始压缩数据的约 2.5 倍（稻香 3106→7704、Lemon 7000→17430、Hello 3928→10332）。
- `kg_mid` 可为随机 32 位 hex，服务端不校验；`duration` 参数可写死 `200000`（实测照样命中 223609ms 的候选）。
- `download` 返回的 `charset` 在 `fmt=krc` 时为空字符串，`fmt=lrc` 时为 `utf8`；LRC 带 UTF-8 BOM（`\ufeff`），写入前建议 strip。

---

## 5. 网易云歌词 —— 可用（无需登录）

```bat
python netease_probe.py "周杰伦 稻香"
python netease_lyric_by_id.py 185809 35847388
```

**搜索（原项目端点，HTTP 200）**：

```
GET http://music.163.com/api/cloudsearch/pc?s=%E5%91%A8%E6%9D%B0%E4%BC%A6%20%E7%A8%BB%E9%A6%99&type=1&offset=0&limit=10
HTTP 200 bytes 15451
code = 200 msg = None
songs = 10
   id=3381542797 | 周杰伦. - 稻香(治愈版) | pop=80.0
   id=3441643322 | Jay - 稻香 | pop=75.0
   id=2742150885 | 沈幼楚 - 稻香(正式版) | pop=25.0
```

**歌词（原项目端点，HTTP 200）**：

```
GET http://music.163.com/api/song/lyric?id=185809&lv=-1&kv=-1&tv=-1
HTTP 200 size=2600 code= 200 keys= ['sgc','sfy','qfy','lyricUser','lrc','klyric','tlyric','code']
lrc 长度 1281  tlyric in resp: True
'[00:00.000] 作词 : 周杰伦'
'[00:01.000] 作曲 : 周杰伦'
'[00:02.000] 编曲 : 林迈可'
'[00:03.000] 制作人 : 周杰伦'
'[00:04.000] 鼓 : 陈柏州'
```

原始响应片段（`netease_lyric_185809.json`）：

```json
{"sgc":false,"sfy":false,"qfy":false,"lyricUser":{"id":508687,"status":99,"demand":0,"userid":547896341,"nickname":"大锦鲤做了错饭hia","uptime":1581413396792},"lrc":{"version":57,"lyric":"[00:00.000] 作词 : 周杰伦\n[00:01.000] 作曲 : 周杰伦\n ...
```

**`tlyric` 验证（英文歌 id=35847388 Adele - Hello，HTTP 200）**：

```
HTTP 200  code = 200  romalrc存在 = False  lrc=2248字 tlyric=1180字
tlyric 前 3 行:
   [by:九九Lrc歌词网～www.99Lrc.net]
   [00:06.220]你好 是我
   [00:11.320]我犹豫着要不要给你来电 不确定多年后你是否还愿相见
```

**变异实验（`python header_variants.py`，均 HTTP 200）**：

```
### 网易云 cloudsearch 不带任何自定义头（仅 UA 缺省）
  HTTP 200  bytes 15451   code = 200
### 网易云 cloudsearch 仅带 UA（无 Referer/Origin）
  HTTP 200  bytes 15451   code = 200
### 老端点 /api/search/get（https，带常规头）
  HTTP 200  bytes 6947   code = 200  songCount = 142
```

结论：**可用**，既没有被要求登录，也没有出现 `code != 200`。
- `lrc.lyric` 与 `tlyric.lyric` 都存在（中文歌 `tlyric` 为空字符串，英文歌非空）；`klyric`、`romalrc` 也可能出现，可忽略。
- `Referer`/`Origin` 可有可无（实测不带也 `code=200`），`http` 与 `https` 等价。
- 备选端点 `https://music.163.com/api/search/get?s=...&type=1&limit=10` 也可用（`code=200`、`result.songCount=142`），但字段名是老结构（`artists[]`，无 `pop`），原项目用的 `cloudsearch/pc`（`ar[]`/`al`/`pop`）仍是首选。
- **注意排序**：`cloudsearch/pc` 对「周杰伦 稻香」的前 10 条**不含原唱**（前 5 条是翻唱/童声），原项目的 `nameSimilarity + pop` 打分逻辑仍必要；不要把 `songs[0]` 当原唱。

---

## 6. QQ 音乐歌词 —— 可用

```bat
python qq_make_body.py "周杰伦 稻香" 003aAYrm3GE0Ac
curl.exe -s -o qq_search.json -w "HTTP=%{http_code} size=%{size_download}\n" -X POST ^
  -H "Referer: https://y.qq.com" -H "Origin: https://y.qq.com" -H "Content-Type: application/json" ^
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36" ^
  --data-binary "@qq_search_body.json" "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json"
curl.exe -s -o qq_lyric.json -w "HTTP=%{http_code} size=%{size_download}\n" -X POST ^
  -H "Referer: https://y.qq.com" -H "Origin: https://y.qq.com" -H "Content-Type: application/json" ^
  -H "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0.0.0 Safari/537.36" ^
  --data-binary "@qq_lyric_body.json" "https://u.y.qq.com/cgi-bin/musicu.fcg?format=json"
python qq_show_lyric.py qq_lyric.json
python qq_get_fallback.py
```

请求体原文（`qq_search_body.json`，与 `qqmusic_provider.cpp` 一致）：

```json
{"comm":{"ct":19,"cv":"1859","uin":"0"},"req":{"method":"DoSearchForQQMusicDesktop","module":"music.search.SearchCgiService","param":{"query":"周杰伦 稻香","num_per_page":10,"page_num":1}}}
```

**POST 搜索：HTTP 200**

```
code= 0  req.code= 0
body keys= ['album', 'gedantip', 'mv', 'qc', 'singer', 'song', 'songlist', 'user', 'zhida']
song.list len= 10
  mid=003aAYrm3GE0Ac | 周杰伦 - 稻香 | listenCnt=None | album.mid=002Neh8l0uciQZ
  mid=003joRpF3wA7e8 | 周杰伦 - 稻香 | listenCnt=None | album.mid=
```

**POST 歌词（`GetPlayLyricInfo`，HTTP 200）**

```
top code = 0  req.code = 0
req.data keys = ['songID','songName','songType','singerName','qrc','crypt','lyric','trans','roma','lrc_t','qrc_t','trans_t','roma_t', ...]
  lyric  : str len=2668
  trans  : str len=0
lyric base64 是否合法 = True  -> 解码 1057 字节
lyric 前 5 行:
   [ti:稻香]
   [ar:周杰伦]
   [al:魔杰座]
   [by:]
   [offset:0]
```

原始响应片段（`qq_lyric.json`，≤20 行）：

```json
{"code":0,"ts":1793504865618,"start_ts":1793504865609,"traceid":"5a5df5b44ced7e9e","req":{"code":0,"data":{"songID":449205,"songName":"","songType":0,"singerName":"","qrc":0,"crypt":0,"lyric":"W3RpOueou+mmmV0KW2FyOuWRqOadsOS8pl0KW2FsOumtlOadsOW6p10KW2J5Ol0KW29mZnNldDowXQpbMDA6MDAuMDBd56i76aaZIC0g5ZGo5p2w5LymIChKYXkgQ2hvdSkKWzAwOjA3LjczXeivje+8muWRqOadsOS8pgpbMDA6MTUuNDdd5puy77ya5ZGo5p2w5LymClswMDo
```

**`trans` 验证（英文歌 mid=002Il7Ya1tZ6UZ，HTTP 200）**：

```
  lyric  : str len=2968
  trans  : str len=3220
lyric base64 是否合法 = True  -> 解码 2224 字节
trans 解码 1519 字节
```

**GET 降级（`python qq_get_fallback.py`）**：

```
GET https://u.y.qq.com/cgi-bin/musicu.fcg?format=json&data=%7B%22comm%22%3A%7B%22ct%22%3A19%2C...%7D%7D
HTTP 200 bytes 7990
code = 0  req.code = 0
lyric len = 2968  trans len = 3220
```

结论：**可用**。POST **没有被拒**（不需要 `sign`/`Sign` 头），`format=json` 查询参数照旧；返回结构仍是 `req.data.body.song.list`，歌词仍是 `req.data.lyric`/`req.data.trans` 的 **base64 字符串**。
- **`listenCnt` 字段已不存在**（`song.list[*]` 里没有播放量字段，见上 `listenCnt=None`），原项目的 `popBonus = 0.15 * listenCnt/maxPop` 会恒为 0 → 排序退化为「名称相似度 + 歌手相似度」，功能不坏但排序会变。
- GET 降级同样 `code=0`、结果字节数略小（44060 vs 45076），可作为备份路径保留。

---

## 7. 汽水音乐 —— `api.qishui.com` 不可用；分享页可用（需 track_id）

```bat
python qishui_probe.py        :: 搜索接口 3 种变异
python qishui_probe2.py       :: track_v2 + 搜索复测 + 分享页
python qishui_shortlink.py    :: 短链 → 真实 track_id → 分享页
python qishui_parse_share.py  :: 解析分享页 audioWithLyricsOption
```

**变异 1：原样（`User-Agent: LunaPC/3.0.0(290101097)` + 原项目 `X-Helios` / `X-Medusa` 全头）**

```
### 变异1: 原样（完整 X-Helios/X-Medusa + LunaPC UA）
  HTTP 200 bytes 0  server = volc-dcdn  echo-trace = 202611011148114602FAF45CD3508D7240
  body 前 400 字:
```

**变异 2：去掉 `X-Helios`/`X-Medusa`**

```
  HTTP 200 bytes 0  server = volc-dcdn  echo-trace = 202611011148114D46C3C805DBF58D411C
```

**变异 3：换浏览器 UA + `Referer: https://music.douyin.com/`**

```
  HTTP 200 bytes 0  server = volc-dcdn  echo-trace = 202611011148113983FCB3DD7CD0896A67
```

响应头原文（`curl.exe -s -i ...`）——**状态码 200，但 `Content-Length: 0`，无任何错误体**：

```
HTTP/1.1 200 OK
Server: volc-dcdn
Content-Type: application/json
Content-Length: 0
Date: Sun, 01 Nov 2026 03:48:16 GMT
x-tt-logid: 20261101114816FF5F3A2F51C2CB8D986B
X-Dsa-Origin-Status: 200
```

`track_v2` 歌词接口（用**真实** `track_id=7562058096069232641` 复测）：

```
GET https://api.qishui.com/luna/pc/track_v2?track_id=7562058096069232641&media_type=track&queue_type=
HTTP 200  Content-Length = 0  body len = 0
```

**分享页备选（可用）**：短链 `https://qishui.douyin.com/s/iu3j3658/` → 302 →

```
  Location = https://music.douyin.com/qishui/share/track?track_id=7562058096069232641&sec_sharer_id=MS4wLjABAAAAjPyf8caj-...
GET https://music.douyin.com/qishui/share/track?track_id=7562058096069232641
  HTTP 200  html len = 159210
```

分享页解析结果（`python qishui_parse_share.py`）：

```
audioWithLyricsOption 长度 = 138225
trackName = 醒着醉（烟嗓版）
artistName = 烟嗓司机
status_code = 0  hasCopyright = True
lyrics 类型 = dict  lyrics keys = ['lyricType', 'sentences']
sentences = 52
   startMs=0 words=抬头看见那
   startMs=3670 words=微笑的雪花
   startMs=8910 words=你别出现在我黎明的梦里
```

结论：**`api.qishui.com` 的两个接口（`luna/pc/search/track`、`luna/pc/track_v2`）不可用**——HTTP **200** 但 **`Content-Length: 0` / 无 body**（`X-Dsa-Origin-Status: 200` 说明是源站自己回了空体，不是 CDN 拦截；3 种变异结果一致，`X-Helios`/`X-Medusa` 是否携带不影响结果 → 这套硬编码头已无意义）。按边界条款不再强行绕过。
- **但分享页这条备选链路仍然有效**：`https://music.douyin.com/qishui/share/track?track_id=<19位数字>` → 200 HTML（159KB），`_ROUTER_DATA` 里内嵌 `audioWithLyricsOption`（138KB JSON），含 `trackName`/`artistName`/`coverURL`/`lyrics.sentences[]`（`startMs`/`endMs`/`text`/`words[]`），原项目 `extractJsonObject("audioWithLyricsOption")` 的花括号配平逻辑**在 138KB 上仍然有效**（< 其 200000 字符上限）。
- 分享页的致命限制：**必须先有 `track_id`**。而唯一的 `track_id` 来源（搜索接口）已失效 → 关键词搜歌这条路断了。可行的落地方式：让用户粘贴汽水分享链接（`qishui.douyin.com/s/xxxx` 或含 `track_id=` 的链接），App 内解析 `track_id` 后走分享页。

---

## 8. B 站音频完整下载 + ffprobe —— 可用

样本：`BV1aB4y1u7ox`（《稻香》吉他指弹，176 秒，落在 1~3 分钟区间）。

```bat
python bili_download_full.py
where ffprobe
ffprobe -v error -show_format -show_streams sample_BV1aB4y1u7ox.m4a > ffprobe_out.txt 2>&1
type ffprobe_out.txt
```

下载过程打印：

```
view: aid=588323544 cid=711474810 duration=176 title=【光遇琴谱】回家吧，回到最初的美好《稻香》[周杰伦][吉他3指]
audio streams = [(30216, 67332), (30232, 132793), (30280, 191089)]  picked = 30280 191089
HTTP 200  Content-Type = application/octet-stream  Content-Length = 4186124
saved ...\docs\probe\sample_BV1aB4y1u7ox.m4a
下载字节数 = 4186124 (3.99 MB)
耗时 = 0.86 秒  (4746 KB/s)
音频比特率估算 = 190 kbps（按 176 秒）
```

`where ffprobe` → `E:\Scoop\shims\ffprobe.exe`（存在）。

`ffprobe` 原始输出前 10 行：

```
[STREAM]
index=0
codec_name=aac
codec_long_name=AAC (Advanced Audio Coding)
profile=LC
codec_type=audio
codec_tag_string=mp4a
codec_tag=0x6134706d
mime_codec_string=mp4a.40.2
sample_fmt=fltp
```

同一次输出的尾部（作为「是否合法 M4A」的判据）：

```
duration=175.253000
bit_rate=189473
sample_rate=48000
channels=2
channel_layout=stereo
...
[FORMAT]
filename=sample_BV1aB4y1u7ox.m4a
nb_streams=1
format_name=mov,mp4,m4a,3gp,3g2,mj2
duration=175.253000
size=4186124
bit_rate=191089
probe_score=100
TAG:major_brand=iso5
TAG:compatible_brands=avc1iso5dsmsmsixdash
TAG:encoder=Lavf57.71.100
TAG:description=Packed by Bilibili XCoder v2.0.2
```

完整文件的顶层 box 序列（`python mp4boxes.py sample_BV1aB4y1u7ox.m4a 2`）：

```
总行数 269  顶层 box 数 = 78
序列前 12: ['file', 'ftyp', 'free', 'free', 'moov', 'sidx', 'moof', 'mdat', 'moof', 'mdat', 'moof', 'mdat']
累计计数: Counter({'moof': 36, 'mdat': 36, 'free': 2, 'ftyp': 1, 'moov': 1, 'sidx': 1})
最后 2 个顶层:
moof   size=136      offset=4179920
mdat   size=6068     offset=4180056
```

结论：**可用**。整段 `.m4s` 字节流 4186124 字节、0.86 秒下完，`ffprobe` 判为 `format_name=mov,mp4,m4a,...`、`probe_score=100`、单条 AAC-LC 48kHz 立体声流、时长 175.25s —— **是合法可播放的 M4A**，文件名直接存 `.m4a` 即可（36 个 `moof+mdat` 分片 + 头部 `moov`，播放器按 `sidx`/`trex` 索引）。

---

## 9. `NarMusic\download\` 历史产物 —— 不存在

```bat
dir /a "E:\桌面\安卓软件\NarMusic\download"
dir /s /b "E:\桌面\安卓软件\NarMusic\download"
dir /s /b "E:\桌面\安卓软件" | findstr /I ".m4a"
```

实际输出：

```
download 目录不存在
----
 驱动器 E 中的卷是 新加卷
 卷的序列号是 549E-0129
 E:\桌面\安卓软件\NarMusic 的目录
找不到文件
----
E:\桌面\安卓软件\NarMusicAndroid\docs\probe\sample_BV1aB4y1u7ox.m4a
```

`dir /a "E:\桌面\安卓软件\NarMusic"` 也没有 `download/`、`output/` 目录（只有 `.git build.sh cmake CMakeLists.txt config.json include lib README.md src web`），整个 `NarMusic` 树内没有任何 `.m4a/.mp4/.mp3/.flac`。

结论：**历史产物不存在**，无法提供「真实产物」的第一手 box 证据。改用下面三条替代证据（本机实测，非推测）：

**(a) 原始流的 ilst 原子（B 站下载样本，见 2.2）**：只有 `©too`、`desc`，没有 `©nam`/`©lyr`/`covr`/`----`。

**(b) 按 `music_file_repository.cpp` 的写法写入标签后应有形态（用 ffmpeg 模拟，`python make_tagged_sample.py`）**：

```
moov: (4148620, 36381)
udta: (4182166, 2835)
meta: (4182174, 2827)
ilst: (4182219, 2782) (meta 为 fullbox，子 box 从 +12 开始)

ilst 内原子：
  atom='©nam'   size=30   data type=1 value='稻香'
  atom='©ART'   size=33   data type=1 value='周杰伦'
  atom='©alb'   size=33   data type=1 value='魔杰座'
  atom='©too'   size=37   data type=1 value='Lavf62.12.101'
  atom='©cmt'   size=36   data type=1 value='narnat-probe'
  atom='©lyr'   size=2205 data type=1 value='[id:$00000000]\n\n[ar:周杰伦]\n\n[ti:稻香]\n\n...'
  atom='desc'   size=56   data type=1 value='Packed by Bilibili XCoder v2.0.2'
  atom='covr'   size=344  data type=13 len=320 binary=ffd8ffe000104a464946000102000001...
```

即目标产物结构 = `moov.udta.meta.ilst` 下 `©nam` / `©ART` / `©alb` / `©lyr` / `covr`（`data type=13` 为 JPEG 封面）；`covr` 的 `data` 类型 13=JPEG、14=PNG。

**(c) 原项目实际写的原子清单（`src/infrastructure/filesystem/music_file_repository.cpp`，TagLib 实现，只读核对）**：
`setTitle→©nam`、`setArtist→©ART`、`setAlbum→©alb`、`©lyr`（歌词）、`covr`（封面）、`----:com.narnat:delayMs`、`----:com.narnat:narmeta`（两个 freeform `----` 原子，需手工构造 `mean`/`name` 子 box，ffmpeg 不会写这类原子，Kotlin 侧若要兼容必须自己实现）。

---

## 10. 给 Kotlin 实现的接口速查表

| 平台 | 方法+URL | 必备头 | 关键字段 | 是否可用 |
|---|---|---|---|---|
| B 站视频信息 | GET `https://api.bilibili.com/x/web-interface/view?bvid=<BV>` | `Referer: https://www.bilibili.com`、常规 UA | `data.aid`(Long)、`data.cid`(Long)、`data.title`、`data.pages[].cid` | 可用 |
| B 站音频直链 | GET `https://api.bilibili.com/x/player/playurl?avid=<aid>&cid=<cid>&qn=0&type=&otype=json&fnver=0&fnval=80` | 同上 | `data.dash.audio[]`：`id`(30216/30232/30280)、`bandwidth`、`baseUrl`、`backupUrl[]`、`mimeType` | 可用 |
| B 站音频下载 | GET `baseUrl`（或 `backupUrl[i]`，`Range` 可断点） | `Referer: https://www.bilibili.com` + UA（缺 → 403） | 字节流即 fMP4（`ftyp→moov→sidx→(moof→mdat)×N`），直接存 `.m4a`；init 段=前 837 字节（该样本） | 可用 |
| B 站 WBI 搜索 | GET `https://api.bilibili.com/x/web-interface/nav`（取 key）→ GET `https://api.bilibili.com/x/web-interface/wbi/search/all/v2?keyword=&page=1&pagesize=20&wts=&w_rid=` | `Referer`、UA；**Cookie 可省** | `code==0`、`data.result[].result_type=="video"`、`data.result[].data[]`：`bvid`/`title`/`duration`("m:ss"字符串)/`play` | 可用 |
| 酷狗歌词 | GET `http://mobilecdn.kugou.com/api/v3/search/song?format=json&keyword=<kw>&page=1&pagesize=10&showtype=1` → GET `http://lyrics.kugou.com/search?ver=1&man=yes&client=pc&keyword=&duration=200000&hash=` → GET `http://lyrics.kugou.com/download?ver=1&client=pc&id=&accesskey=&fmt=lrc|krc&charset=utf8` | `Referer: http://www.kugou.com`、`Cookie: kg_mid=<32hex>` | `status==1`；`data.info[].hash`/`songname`/`singername`/`album_id`；`candidates[].id`/`accesskey`；`content`(base64)；**无 `trans` 字段**，翻译在 KRC `[language:]` | 可用 |
| 网易云歌词 | GET `http://music.163.com/api/cloudsearch/pc?s=<kw>&type=1&offset=0&limit=10` → GET `http://music.163.com/api/song/lyric?id=<id>&lv=-1&kv=-1&tv=-1` | 无必需头（带 `Referer/Origin` 也行） | `code==200`、`result.songs[]`：`id`/`name`/`ar[0].name`/`al.name`/`al.picUrl`/`pop`；`lrc.lyric`、`tlyric.lyric` | 可用 |
| QQ 音乐歌词 | POST `https://u.y.qq.com/cgi-bin/musicu.fcg?format=json`（body=`DoSearchForQQMusicDesktop` → 再 `GetPlayLyricInfo`）；降级 GET `...&data=<urlencode(body)>` | `Referer: https://y.qq.com`、`Origin: https://y.qq.com`、`Content-Type: application/json` | `req.data.body.song.list[]`：`mid`/`name`/`singer[0].name`/`album.mid`；`req.data.lyric`、`req.data.trans`（均 base64）；**无 `listenCnt`** | 可用 |
| 汽水音乐搜索/歌词 API | GET `https://api.qishui.com/luna/pc/search/track?...`、GET `https://api.qishui.com/luna/pc/track_v2?track_id=` | 原 `X-Helios`/`X-Medusa` 已无效（带不带都同结果） | 无 body（`Content-Length: 0`），无字段可取 | **不可用** |
| 汽水音乐分享页 | GET `https://music.douyin.com/qishui/share/track?track_id=<19位>` | `Referer: https://music.douyin.com/`、H5 UA | HTML 内 `_ROUTER_DATA` → `audioWithLyricsOption`：`trackName`/`artistName`/`coverURL`/`lyrics.sentences[].{startMs,endMs,text,words[]}` | 可用（需先有 track_id；短链 `qishui.douyin.com/s/xxx` 可解析出 track_id） |

---

## 11. 主动披露：与 `NarMusic` 源码实现不一致的地方

1. **QQ 搜索的 `listenCnt` 字段已不存在**（`qqmusic_provider.cpp:100` 的 `songs[i].value("listenCnt", 0LL)`）。实测 `song.list[*]` 无该字段（也无等价播放量字段），`popBonus` 恒为 0。
2. **酷狗 `download` 响应已无 `trans` 字段**（`kugou_provider.cpp:222` 的 `if (lyricData.contains("trans"))` 永远不成立）。翻译只存在于 KRC 的 `[language:<base64>]` → 所以原项目「LRC 无翻译则回落 KRC」的流程实际上**每次都走 KRC**，行为正确但多一次请求；Kotlin 侧可直接跳过 `trans` 判断。
3. **B 站 WBI 的硬编码 fallback 与当前真实密钥完全相同**（`bilibili_client.cpp:70-71` 的 `7cd08494…` / `4932caff…` 恰好等于今天 `nav` 返回的 `img_key`/`sub_key`）。这不是稳定保证，只是当下巧合；且 `nav` 未登录返回 `code=-101`，`fetchWbiKeys` 只判 `data.wbi_img` 存在与否是对的，**不要用 `code==0` 判断**。
4. **B 站搜索 `duration` 是 `"419:2"`/`"3:43"` 字符串**，不是数字秒。原项目已兼容两种分支，Kotlin 侧不能简化成 `asInt`。
5. **B 站 `aid` 已到 15 位**（`116849853337990`），超过 `Int` 上限；`bilibili_client.cpp` 用 `long long` 没问题，Kotlin 必须 `Long`。
6. **汽水音乐主链路（搜索 API）已死**，`qishui_provider.cpp::searchSong` 整段失效；只有 `getLyricsViaSharePage` 这条路还能用，且 `track_id` 只能从用户分享链接里来，不能再从关键词搜索得到。此外分享页 JSON 里 `lyrics.sentences[]` 无 `translatedWords`（实测 0/52 条有翻译），`krcJsonToLrc` 的翻译分支实际取不到数据。
7. **B 站音频下载接口的 `Content-Type` 是 `video/mp4`**（`application/octet-stream` 视 CDN 而定），且响应另带 `mimeType: audio/mp4` 只在 JSON 里；Kotlin 别用响应头判断类型。
8. 无签名的 QQ `POST` 目前**不需要** `sign` 参数（与源码一致），但 GET 降级也可用，两种都实测 200。

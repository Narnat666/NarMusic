// ==================== 核心下载功能 (完全保留原逻辑) ====================
let pollingInterval = null;
let currentTaskId = null;
let lastSpeedUpdateTime = Date.now();
let lastSpeedBytes = 0;
let speedHistory = [];
const MAX_SPEED_HISTORY = 3;
let downloadStartTime = null;
let taskSuccessNotified = false;

// 音乐库相关变量
let musicLibrary = [];
let isLibraryLoading = false;

const urlInput = document.getElementById('urlInput');
const filenameInput = document.getElementById('filenameInput');
const sendBtn = document.getElementById('sendBtn');
const localDownloadBtn = document.getElementById('localDownloadBtn');
const statusCard = document.getElementById('statusCard');
const statusLight = document.getElementById('statusLight');
const taskStatus = document.getElementById('taskStatus');
const downloadedBytesEl = document.getElementById('downloadedBytes');
const downloadSpeedEl = document.getElementById('downloadSpeed');
const downloadTimeEl = document.getElementById('downloadTime');
const downloadStatusEl = document.getElementById('downloadStatus');
// 设置界面元素
const settingsPlatformSelect = document.getElementById('settingsPlatformSelect');
const settingsDelayInput = document.getElementById('settingsDelayInput');
const saveSettingsBtn = document.getElementById('saveSettingsBtn');
const resetSettingsBtn = document.getElementById('resetSettingsBtn');
const fileInfo = document.getElementById('fileInfo');
const fileNameEl = document.getElementById('fileName');
const fileSizeEl = document.getElementById('fileSize');
const toast = document.getElementById('toast');

// 音频播放器相关变量（用于库模块音乐播放）
let audioPlayer = null;
let audioSection = null;
let isPlaying = false;
let playerTitle = null;
let playerArtist = null;

// 歌词相关变量
let currentLyrics = []; // 数组格式: {time: seconds, text: string}
let lyricsDisplay = null;

function formatBytes(bytes) {
    if (bytes === 0 || bytes === undefined) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    if (i === 0) return bytes + ' ' + sizes[i];
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatSpeed(bytesPerSecond) {
    if (bytesPerSecond === 0) return '0 B/s';
    const k = 1024;
    const sizes = ['B/s', 'KB/s', 'MB/s'];
    const i = Math.floor(Math.log(bytesPerSecond) / Math.log(k));
    if (i >= sizes.length) i = sizes.length - 1;
    if (i === 0) return bytesPerSecond + ' ' + sizes[i];
    return parseFloat((bytesPerSecond / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function formatTime(seconds) {
    if (seconds < 60) return seconds.toFixed(1) + '秒';
    if (seconds < 3600) return Math.floor(seconds / 60) + '分' + Math.floor(seconds % 60) + '秒';
    return Math.floor(seconds / 3600) + '小时' + Math.floor((seconds % 3600) / 60) + '分';
}

function formatDuration(seconds) {
    if (!seconds || isNaN(seconds)) return '00:00';
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return String(mins).padStart(2, '0') + ':' + String(secs).padStart(2, '0');
}

function extractUrlFromText(text) {
    const urlPattern = /(https?:\/\/[^\s]+|www\.[^\s]+|[^\s]+\.[a-z]{2,}\/[^\s]*)/gi;
    const matches = text.match(urlPattern);
    if (matches && matches.length > 0) {
        let url = matches[0];
        if (!url.startsWith('http://') && !url.startsWith('https://')) {
            url = 'https://' + url;
        }
        return url;
    }
    return null;
}

function showToast(message, type = 'info') {
    // 如果已经有Toast正在显示，先隐藏它
    if (toast.classList.contains('show')) {
        toast.classList.remove('show');
        setTimeout(() => {
            showNewToast(message, type);
        }, 300);
    } else {
        showNewToast(message, type);
    }
}

function showNewToast(message, type) {
    toast.textContent = message;
    toast.className = 'toast ' + type;
    setTimeout(() => toast.classList.add('show'), 10);
    setTimeout(() => {
        toast.classList.remove('show');
    }, 3000);
}

// 设置管理功能
function loadSettings() {
    const savedPlatform = localStorage.getItem('musicPlatform');
    const savedDelay = localStorage.getItem('downloadDelay');

    if (savedPlatform && settingsPlatformSelect) {
        settingsPlatformSelect.value = savedPlatform;
    }

    if (savedDelay && settingsDelayInput) {
        settingsDelayInput.value = savedDelay;
    }
}

function saveSettings() {
    if (settingsPlatformSelect) {
        localStorage.setItem('musicPlatform', settingsPlatformSelect.value);
    }

    if (settingsDelayInput) {
        localStorage.setItem('downloadDelay', settingsDelayInput.value);
    }

    showToast('设置已保存', 'success');
}

function resetSettings() {
    if (settingsPlatformSelect) {
        settingsPlatformSelect.value = '网易云音乐';
    }

    if (settingsDelayInput) {
        settingsDelayInput.value = '0';
    }

    localStorage.removeItem('musicPlatform');
    localStorage.removeItem('downloadDelay');

    showToast('设置已恢复默认', 'info');
}

// 初始化音频播放器
function initAudioPlayer() {
    audioPlayer = document.getElementById('audioPlayer');
    audioSection = document.getElementById('audioSection');
    playerTitle = document.getElementById('playerTitle');
    playerArtist = document.getElementById('playerArtist');
    lyricsDisplay = document.getElementById('lyricsDisplay');
    const closeAudioBtn = document.getElementById('closeAudioBtn');

    if (closeAudioBtn) {
        closeAudioBtn.addEventListener('click', (e) => {
            e.stopPropagation(); // 阻止事件冒泡
            e.preventDefault(); // 阻止默认行为

            audioSection.classList.remove('show');
            audioPlayer.pause();
            isPlaying = false;
        });
    }

    if (audioPlayer) {
        audioPlayer.addEventListener('play', () => {
            isPlaying = true;
        });

        audioPlayer.addEventListener('pause', () => {
            isPlaying = false;
        });

        audioPlayer.addEventListener('ended', () => {
            isPlaying = false;
        });

        audioPlayer.addEventListener('timeupdate', updateLyricsHighlight);
    }

    // 添加拖动功能
    initDragFunctionality();
}

// 初始化拖动功能
function initDragFunctionality() {
    const audioPopup = document.getElementById('audioSection');
    const header = document.querySelector('.audio-popup-header');

    if (!audioPopup || !header) return;

    let isDragging = false;
    let xOffset = 0;
    let yOffset = 0;
    let lastTouchTime = 0;
    const TOUCH_DELAY = 16; // 约60fps

    // 鼠标按下事件
    header.addEventListener('mousedown', dragStart);

    // 触摸开始事件（移动端支持）
    header.addEventListener('touchstart', dragStart, { passive: false });

    // 鼠标移动事件
    document.addEventListener('mousemove', drag);

    // 触摸移动事件（移动端支持）
    document.addEventListener('touchmove', drag, { passive: false });

    // 鼠标释放事件
    document.addEventListener('mouseup', dragEnd);

    // 触摸结束事件（移动端支持）
    document.addEventListener('touchend', dragEnd);

    function dragStart(e) {
        // 如果是关闭按钮，不触发拖动
        if (e.target.closest('#closeAudioBtn')) {
            return;
        }

        let clientX, clientY;

        if (e.type === 'touchstart') {
            e.preventDefault();
            clientX = e.touches[0].clientX;
            clientY = e.touches[0].clientY;
        } else {
            clientX = e.clientX;
            clientY = e.clientY;
        }

        // 获取当前弹窗位置
        const rect = audioPopup.getBoundingClientRect();
        xOffset = clientX - rect.left;
        yOffset = clientY - rect.top;

        if (e.target === header || header.contains(e.target)) {
            isDragging = true;

            // 添加拖动时的样式
            audioPopup.style.transition = 'none';
            header.style.cursor = 'grabbing';
        }
    }

    function drag(e) {
        if (!isDragging) return;

        e.preventDefault();

        // 移动端防抖，提高流畅度
        if (e.type === 'touchmove') {
            const now = Date.now();
            if (now - lastTouchTime < TOUCH_DELAY) {
                return;
            }
            lastTouchTime = now;
        }

        let clientX, clientY;

        if (e.type === 'touchmove') {
            clientX = e.touches[0].clientX;
            clientY = e.touches[0].clientY;
        } else {
            clientX = e.clientX;
            clientY = e.clientY;
        }

        // 计算新位置
        const newX = clientX - xOffset;
        const newY = clientY - yOffset;

        // 限制在可视区域内
        const viewportWidth = window.innerWidth;
        const viewportHeight = window.innerHeight;
        const popupWidth = audioPopup.offsetWidth;
        const popupHeight = audioPopup.offsetHeight;

        const boundedX = Math.max(0, Math.min(newX, viewportWidth - popupWidth));
        const boundedY = Math.max(0, Math.min(newY, viewportHeight - popupHeight));

        // 应用新位置
        audioPopup.style.left = boundedX + 'px';
        audioPopup.style.top = boundedY + 'px';
        audioPopup.style.transform = 'translate(0, 0)';
    }

    function dragEnd() {
        if (!isDragging) return;

        isDragging = false;

        // 恢复过渡效果
        audioPopup.style.transition = 'transform 0.3s ease, opacity 0.3s ease';
        header.style.cursor = 'move';

        // 保存位置到本地存储
        savePopupPosition();
    }

    // 保存弹窗位置到本地存储
    function savePopupPosition() {
        const left = parseInt(audioPopup.style.left) || 0;
        const top = parseInt(audioPopup.style.top) || 0;
        const position = {
            x: left,
            y: top
        };
        localStorage.setItem('audioPopupPosition', JSON.stringify(position));
    }

    // 加载弹窗位置
    function loadPopupPosition() {
        const savedPosition = localStorage.getItem('audioPopupPosition');
        if (savedPosition) {
            const position = JSON.parse(savedPosition);

            // 确保位置在可视区域内
            const viewportWidth = window.innerWidth;
            const viewportHeight = window.innerHeight;
            const popupWidth = audioPopup.offsetWidth;
            const popupHeight = audioPopup.offsetHeight;

            let x = position.x;
            let y = position.y;

            // 限制在可视区域内
            x = Math.max(0, Math.min(x, viewportWidth - popupWidth));
            y = Math.max(0, Math.min(y, viewportHeight - popupHeight));

            // 应用位置
            audioPopup.style.top = y + 'px';
            audioPopup.style.left = x + 'px';
            audioPopup.style.transform = 'translate(0, 0)';
        }
    }

    // 初始加载位置
    loadPopupPosition();

    // 窗口大小改变时重新调整位置
    window.addEventListener('resize', () => {
        loadPopupPosition();
    });
}

// 辅助函数：使用指定Range请求并解析歌词
function fetchLyricsWithRange(filename, rangeStart, rangeEnd, attempt) {
    return new Promise((resolve, reject) => {
        const url = `/api/download/stream?filename=${encodeURIComponent(filename)}`;
        const rangeHeader = rangeStart === null ? 'none' : `bytes=${rangeStart}-${rangeEnd}`;
        
        console.log(`[歌词调试] 尝试${attempt}，Range: ${rangeHeader}`);
        
        const options = {};
        if (rangeStart !== null) {
            options.headers = { 'Range': rangeHeader };
        }
        
        fetch(url, options)
        .then(response => {
            console.log(`[歌词调试] 尝试${attempt}响应状态:`, response.status, response.statusText);
            
            if (!response.ok) {
                // 如果Range请求失败，尝试完整请求
                if (response.status === 416 && rangeStart !== null) {
                    console.log(`[歌词调试] 尝试${attempt} Range请求失败(416)，尝试完整请求`);
                    return fetch(url); // 不带Range头
                }
                throw new Error(`HTTP ${response.status} - ${response.statusText}`);
            }
            return response.blob();
        })
        .then(blob => {
            console.log(`[歌词调试] 尝试${attempt}获取到Blob，大小:`, blob.size, '类型:', blob.type);
            
            if (!blob || blob.size === 0) {
                reject(new Error('Blob为空或无效'));
                return;
            }
            
            jsmediatags.read(blob, {
                onSuccess: function(tag) {
                    console.log(`[歌词调试] 尝试${attempt} jsmediatags解析成功`);
                    console.log(`[歌词调试] 尝试${attempt} 可用标签:`, Object.keys(tag.tags));
                    
                    const lyrics = tag.tags.lyrics;
                    if (lyrics) {
                        console.log(`[歌词调试] 尝试${attempt} 找到歌词，长度:`, lyrics.length);
                        resolve(lyrics);
                    } else {
                        console.log(`[歌词调试] 尝试${attempt} 未找到歌词标签`);
                        resolve(null);
                    }
                },
                onError: function(error) {
                    console.error(`[歌词调试] 尝试${attempt} jsmediatags解析失败:`, error);
                    console.error(`[歌词调试] 尝试${attempt} 错误类型:`, error.type);
                    console.error(`[歌词调试] 尝试${attempt} 错误信息:`, error.info || error.message || error);
                    reject(error);
                }
            });
        })
        .catch(error => {
            console.error(`[歌词调试] 尝试${attempt} 请求失败:`, error);
            console.error(`[歌词调试] 尝试${attempt} 错误消息:`, error.message || error);
            reject(error);
        });
    });
}

// 歌词相关功能
 async function fetchLyrics(filename) {
     console.log('[歌词调试] 开始加载歌词，文件名:', filename);
     
     // 检查jsmediatags库是否加载
     if (typeof jsmediatags === 'undefined') {
         const error = new Error('jsmediatags库未加载');
         console.error('[歌词调试] jsmediatags库未加载');
         throw error;
     }
     
     // 策略1：先尝试文件开头512KB
     console.log('[歌词调试] 尝试策略1: 文件开头512KB');
     try {
         const lyrics1 = await fetchLyricsWithRange(filename, 0, 524287, '1(开头512KB)');
         if (lyrics1 !== null) {
             console.log('[歌词调试] 策略1成功，返回歌词');
             return lyrics1;
         }
     } catch (error) {
         console.log('[歌词调试] 策略1解析失败（可能数据不完整），继续尝试:', error.message || error);
     }
     
     // 策略2：尝试文件末尾512KB
     console.log('[歌词调试] 尝试策略2: 文件末尾512KB');
     try {
         // 获取文件大小 - 使用Range请求
         let fileSize = null;
         const url = `/api/download/stream?filename=${encodeURIComponent(filename)}`;
         
         try {
             const rangeResponse = await fetch(url, { headers: { 'Range': 'bytes=0-0' } });
             const contentRange = rangeResponse.headers.get('Content-Range');
             if (contentRange) {
                 // Content-Range格式: bytes 0-0/1993074
                 const match = contentRange.match(/bytes \d+-\d+\/(\d+)/);
                 if (match) {
                     fileSize = parseInt(match[1]);
                     console.log('[歌词调试] 通过Range获取文件大小:', fileSize);
                 }
             }
         } catch (rangeError) {
             console.log('[歌词调试] Range请求获取文件大小失败:', rangeError.message || rangeError);
         }
         
         if (fileSize && fileSize > 524288) { // 文件大于512KB
             // 计算末尾512KB的范围
             const rangeStart = Math.max(0, fileSize - 524288);
             const rangeEnd = fileSize - 1;
             
             console.log(`[歌词调试] 文件大小${fileSize}，请求末尾范围: ${rangeStart}-${rangeEnd}`);
             const lyrics2 = await fetchLyricsWithRange(filename, rangeStart, rangeEnd, '2(末尾512KB)');
             if (lyrics2 !== null) {
                 console.log('[歌词调试] 策略2成功，返回歌词');
                 return lyrics2;
             }
         } else if (fileSize) {
             console.log(`[歌词调试] 文件大小${fileSize}小于512KB，直接尝试完整文件`);
             // 文件较小，直接尝试完整文件
             const lyrics2 = await fetchLyricsWithRange(filename, null, null, '2(完整文件)');
             if (lyrics2 !== null) {
                 console.log('[歌词调试] 策略2成功，返回歌词');
                 return lyrics2;
             }
         } else {
             console.log('[歌词调试] 无法获取文件大小，跳过策略2');
         }
     } catch (error) {
         console.log('[歌词调试] 策略2失败，继续尝试:', error.message || error);
     }
     
     // 策略3：尝试完整文件
     console.log('[歌词调试] 尝试策略3: 完整文件');
     try {
         const lyrics3 = await fetchLyricsWithRange(filename, null, null, '3(完整文件)');
         if (lyrics3 !== null) {
             console.log('[歌词调试] 策略3成功，返回歌词');
             return lyrics3;
         } else {
             console.log('[歌词调试] 所有策略都未找到歌词');
             return null;
         }
     } catch (error) {
         console.error('[歌词调试] 策略3也失败:', error);
         throw error;
     }
 }

function parseLrcLyrics(lrcText) {
    console.log('[歌词调试] 开始解析LRC歌词');
    console.log('[歌词调试] 原始歌词文本长度:', lrcText.length);
    console.log('[歌词调试] 歌词前200字符:', lrcText.substring(0, 200));
    
    const lines = lrcText.split('\n');
    console.log('[歌词调试] 总行数:', lines.length);
    
    const lyrics = [];
    const timeRegex = /\[(\d+):(\d+\.?\d*)\]/g;
    
    let parsedLines = 0;
    let skippedLines = 0;
    
    for (const line of lines) {
        const matches = [...line.matchAll(timeRegex)];
        if (matches.length === 0) {
            skippedLines++;
            continue;
        }
        
        const text = line.replace(timeRegex, '').trim();
        if (!text) {
            skippedLines++;
            continue;
        }
        
        // 支持多时间戳（双语歌词）
        for (const match of matches) {
            const minutes = parseFloat(match[1]);
            const seconds = parseFloat(match[2]);
            const time = minutes * 60 + seconds;
            lyrics.push({ time, text });
            parsedLines++;
        }
    }
    
    console.log('[歌词调试] 解析结果: 成功解析', parsedLines, '行，跳过', skippedLines, '行');
    
    // 按时间排序
    lyrics.sort((a, b) => a.time - b.time);
    
    if (lyrics.length > 0) {
        console.log('[歌词调试] 第一行歌词:', lyrics[0]);
        console.log('[歌词调试] 最后一行歌词:', lyrics[lyrics.length - 1]);
    } else {
        console.log('[歌词调试] 警告: 未解析到任何歌词行');
    }
    
    return lyrics;
}

function displayLyrics(lyricsArray) {
    console.log('[歌词调试] 开始显示歌词');
    console.log('[歌词调试] 歌词数组长度:', lyricsArray.length);
    
    if (!lyricsDisplay) {
        console.error('[歌词调试] 错误: lyricsDisplay元素未找到');
        return;
    }
    
    if (lyricsArray.length === 0) {
        console.log('[歌词调试] 显示"暂无歌词"');
        lyricsDisplay.innerHTML = '<div class="lyrics-line">暂无歌词</div>';
        return;
    }
    
    console.log('[歌词调试] 生成歌词HTML，共', lyricsArray.length, '行');
    let html = '';
    for (const item of lyricsArray) {
        html += `<div class="lyrics-line" data-time="${item.time}">${item.text}</div>`;
    }
    lyricsDisplay.innerHTML = html;
    
    console.log('[歌词调试] 歌词显示完成，HTML长度:', html.length);
}

function updateLyricsHighlight() {
    if (!audioPlayer || currentLyrics.length === 0 || !lyricsDisplay) return;
    
    const currentTime = audioPlayer.currentTime;
    const lines = lyricsDisplay.querySelectorAll('.lyrics-line');
    
    let activeIndex = -1;
    for (let i = 0; i < currentLyrics.length; i++) {
        if (currentTime >= currentLyrics[i].time) {
            activeIndex = i;
        } else {
            break;
        }
    }
    
    // 移除所有激活状态
    lines.forEach(line => line.classList.remove('active'));
    
    // 激活当前行
    if (activeIndex >= 0 && lines[activeIndex]) {
        lines[activeIndex].classList.add('active');
        // 滚动到可见区域
        lines[activeIndex].scrollIntoView({
            behavior: 'smooth',
            block: 'center'
        });
    }
}

 // 确保弹窗在可视区域内
 function ensurePopupInViewport() {
     const audioPopup = document.getElementById('audioSection');
     if (!audioPopup) return;

     const viewportWidth = window.innerWidth;
     const viewportHeight = window.innerHeight;
     const popupWidth = audioPopup.offsetWidth;
     const popupHeight = audioPopup.offsetHeight;

     // 获取当前弹窗位置
     let currentLeft = parseInt(audioPopup.style.left) || 0;
     let currentTop = parseInt(audioPopup.style.top) || 0;

     // 如果位置未设置，使用默认位置（屏幕右下角）
     if (!audioPopup.style.left || !audioPopup.style.top) {
         currentLeft = viewportWidth - popupWidth - 20;
         currentTop = viewportHeight - popupHeight - 20;

         audioPopup.style.left = currentLeft + 'px';
         audioPopup.style.top = currentTop + 'px';
         audioPopup.style.transform = 'translate(0, 0)';
     }

     // 确保弹窗在可视区域内
     let newLeft = currentLeft;
     let newTop = currentTop;

     // 限制在可视区域内
     newLeft = Math.max(0, Math.min(newLeft, viewportWidth - popupWidth));
     newTop = Math.max(0, Math.min(newTop, viewportHeight - popupHeight));

     // 如果位置有变化，更新位置
     if (newLeft !== currentLeft || newTop !== currentTop) {
         audioPopup.style.left = newLeft + 'px';
         audioPopup.style.top = newTop + 'px';
         audioPopup.style.transform = 'translate(0, 0)';
     }
 }

// 初始化设置
document.addEventListener('DOMContentLoaded', () => {
    loadSettings();
    initAudioPlayer();

    // 设置按钮事件
    if (saveSettingsBtn) {
        saveSettingsBtn.addEventListener('click', saveSettings);
    }

    if (resetSettingsBtn) {
        resetSettingsBtn.addEventListener('click', resetSettings);
    }

    // 音乐库刷新按钮事件
    const refreshLibraryBtn = document.getElementById('refreshLibraryBtn');
    if (refreshLibraryBtn) {
        refreshLibraryBtn.addEventListener('click', loadMusicLibrary);
    }

    // 初始提示
    setTimeout(() => showToast('🎵 输入链接或包含链接的文本即可开始下载', 'info'), 1000);
});

function updateStatusLight(status) {
    statusLight.className = 'status-light ' + status;
    switch(status) {
        case 'error':
            taskStatus.textContent = '下载未完成';
            downloadStatusEl.textContent = '未完成';
            break;
        case 'downloading':
            taskStatus.textContent = '下载中';
            downloadStatusEl.textContent = '进行中';
            break;
        case 'success':
            taskStatus.textContent = '下载完成';
            downloadStatusEl.textContent = '已完成';
            break;
        default:
            taskStatus.textContent = '等待中';
            downloadStatusEl.textContent = '-';
    }
}

function resetTaskStatus() {
    downloadedBytesEl.textContent = '0 B';
    downloadSpeedEl.textContent = '0 B/s';
    taskStatus.textContent = '等待中';
    downloadTimeEl.textContent = '-';
    downloadStatusEl.textContent = '-';
    fileInfo.classList.remove('show');
    updateStatusLight('idle');
    localDownloadBtn.style.display = 'none';
    localDownloadBtn.disabled = true;
    lastSpeedBytes = 0;
    lastSpeedUpdateTime = Date.now();
    speedHistory = [];
    downloadStartTime = null;
    taskSuccessNotified = false;
}

function updateTaskStatus(status) {
    if (status.error) {
        taskStatus.textContent = '问题: ' + status.error;
        updateStatusLight('error');
        stopPolling();
        showToast('下载未完成: ' + status.error, 'warning');
        return;
    }
    if (status.is_downloading) {
        updateStatusLight('downloading');
        localDownloadBtn.style.display = 'none';
    } else if (status.is_finished) {
        if (status.is_success) {
            updateStatusLight('success');
            localDownloadBtn.style.display = 'inline-flex';
            localDownloadBtn.disabled = false;
            if (status.file_info) {
                fileNameEl.textContent = status.file_info.filename || '未命名文件';
                fileSizeEl.textContent = formatBytes(status.file_info.filesize || 0);
                if (downloadStartTime) {
                    downloadTimeEl.textContent = formatTime((Date.now() - downloadStartTime) / 1000);
                }
                fileInfo.classList.add('show');
            }
            if (!taskSuccessNotified) {
                taskSuccessNotified = true;
                showToast('文件下载完成', 'success');
            }
        } else {
            updateStatusLight('error');
            localDownloadBtn.style.display = 'none';
            showToast('下载未完成', 'warning');
        }
        setTimeout(() => stopPolling(), 2000);
    } else {
        updateStatusLight('idle');
        localDownloadBtn.style.display = 'none';
    }
    const downloadedBytes = status.downloaded_bytes || 0;
    downloadedBytesEl.textContent = formatBytes(downloadedBytes);
    const now = Date.now();
    const timeDiff = (now - lastSpeedUpdateTime) / 1000;
    if (timeDiff >= 1) {
        const bytesDiff = downloadedBytes - lastSpeedBytes;
        const speed = bytesDiff / timeDiff;
        speedHistory.push(speed);
        if (speedHistory.length > MAX_SPEED_HISTORY) speedHistory.shift();
        const avgSpeed = speedHistory.reduce((sum, val) => sum + val, 0) / speedHistory.length;
        downloadSpeedEl.textContent = formatSpeed(avgSpeed);
        lastSpeedBytes = downloadedBytes;
        lastSpeedUpdateTime = now;
    }

    // 更新下载时间（在下载过程中就显示）
    if (downloadStartTime) {
        const elapsedSeconds = (Date.now() - downloadStartTime) / 1000;
        downloadTimeEl.textContent = formatTime(elapsedSeconds);
    }
}

function startPollingTaskStatus() {
    if (pollingInterval) clearInterval(pollingInterval);
    downloadStartTime = Date.now();
    pollingInterval = setInterval(async () => {
        if (!currentTaskId) { stopPolling(); return; }
        try {
            const response = await fetch('/api/download/status?task_id=' + currentTaskId);
            if (response.ok) {
                updateTaskStatus(await response.json());
            } else if (response.status === 404) {
                showToast('任务不存在或已过期', 'warning');
                updateStatusLight('error');
                stopPolling();
            }
        } catch (error) {
            console.error('轮询错误:', error);
        }
    }, 1000);
}

function stopPolling() {
    if (pollingInterval) { clearInterval(pollingInterval); pollingInterval = null; }
}

sendBtn.addEventListener('click', async function() {
    const inputText = urlInput.value.trim();
    if (!inputText) { showToast('请输入内容', 'warning'); urlInput.focus(); return; }
    const url = extractUrlFromText(inputText);
    if (!url) { showToast('未找到有效的链接', 'warning'); urlInput.focus(); return; }
    // 从设置界面获取平台和延迟值
    const platform = settingsPlatformSelect ? settingsPlatformSelect.value : "网易云音乐";
    let delayValue = 0;
    if (settingsDelayInput) {
        const delayText = settingsDelayInput.value.trim();
        if (delayText !== '') {
            const parsedDelay = parseInt(delayText);
            if (isNaN(parsedDelay)) { showToast('延迟参数必须是整数', 'warning'); return; }
            delayValue = parsedDelay;
        }
    }
    resetTaskStatus();
    statusCard.style.display = 'block';
    showToast('正在创建下载任务...', 'info');
    sendBtn.disabled = true;
    const originalBtnContent = sendBtn.innerHTML;
    sendBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">refresh</span> 发送中...';
    try {
        const requestData = { content: inputText, url: url, platform: platform, offsetMs: delayValue };
        const filename = filenameInput.value.trim();
        if (filename) requestData.filename = filename;
        const response = await fetch('/api/message', { method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(requestData) });
        const data = await response.json();
        if (response.ok) {
            currentTaskId = data.task_id;
            showToast('下载任务已创建，开始下载...', 'success');
            updateStatusLight('downloading');
            setTimeout(() => startPollingTaskStatus(), 500);
        } else {
            showToast('下载未完成: ' + (data.error || '未知错误'), 'warning');
            updateStatusLight('error');
        }
    } catch (error) {
        showToast('连接错误: ' + error.message, 'warning');
        updateStatusLight('error');
    } finally {
        sendBtn.disabled = false;
        sendBtn.innerHTML = originalBtnContent;
    }
});

localDownloadBtn.addEventListener('click', async function() {
    if (!currentTaskId) { showToast('没有可下载的文件', 'warning'); return; }
    showToast('正在准备文件...', 'info');
    localDownloadBtn.disabled = true;
    const originalContent = localDownloadBtn.innerHTML;
    localDownloadBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">refresh</span> 准备中...';
    try {
        const response = await fetch('/api/download/file?task_id=' + currentTaskId);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let filename = 'downloaded_music';
            if (contentDisposition) {
                const matches = /filename="?([^"]+)"?/i.exec(contentDisposition);
                if (matches && matches[1]) filename = matches[1];
            }
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url; a.download = filename;
            document.body.appendChild(a); a.click();
            window.URL.revokeObjectURL(url); document.body.removeChild(a);
            showToast('文件开始下载到您的设备', 'success');
        } else {
            const errorData = await response.json();
            showToast('下载未完成: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('下载错误: ' + error.message, 'warning');
    } finally {
        localDownloadBtn.disabled = false;
        localDownloadBtn.innerHTML = originalContent;
    }
});



const searchInput = document.getElementById('searchInput');
const searchIcon = document.getElementById('searchIcon');
const searchBar = document.getElementById('searchBar');
let isSearching = false;

async function performSearch() {
    const keyword = searchInput.value.trim();
    if (!keyword) { showToast('请输入搜索关键词', 'warning'); searchInput.focus(); return; }
    if (isSearching) return;

    isSearching = true;
    searchIcon.textContent = 'hourglass_top';
    searchIcon.style.animation = 'spin 1s linear infinite';
    searchInput.disabled = true;

    try {
        const response = await fetch('/api/search?keyword=' + encodeURIComponent(keyword));
        const data = await response.json();

        if (response.ok && data.link) {
            switchTabByName('download');
            urlInput.value = data.title + ' ' + data.link;
            filenameInput.value = data.keyword;
            showToast('已找到: ' + data.title, 'success');
        } else {
            showToast('未找到匹配视频，请尝试其他关键词', 'warning');
        }
    } catch (error) {
        showToast('搜索失败: ' + error.message, 'warning');
    } finally {
        isSearching = false;
        searchIcon.textContent = 'search';
        searchIcon.style.animation = '';
        searchInput.disabled = false;
    }
}

searchInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') { e.preventDefault(); e.stopPropagation(); performSearch(); }
});
searchInput.addEventListener('keyup', (e) => {
    if (e.key === 'Enter') { e.preventDefault(); }
});
searchIcon.addEventListener('click', performSearch);
searchIcon.style.cursor = 'pointer';

function switchTabByName(tabName) {
    document.querySelectorAll('.tab-content').forEach(tab => tab.style.display = 'none');
    document.getElementById(tabName + 'Tab').style.display = 'block';
    document.querySelectorAll('.nav-item').forEach(item => item.classList.remove('active'));
    const navItems = document.querySelectorAll('.nav-item');
    const tabIndex = { download: 0, library: 1, settings: 2 };
    if (tabIndex[tabName] !== undefined && navItems[tabIndex[tabName]]) {
        navItems[tabIndex[tabName]].classList.add('active');
    }
}

function switchTab(tabName) {
    document.querySelectorAll('.tab-content').forEach(tab => tab.style.display = 'none');
    document.getElementById(tabName + 'Tab').style.display = 'block';
    document.querySelectorAll('.nav-item').forEach(item => item.classList.remove('active'));
    event.currentTarget.classList.add('active');

    // 切换到音乐库标签时自动加载音乐库
    if (tabName === 'library') {
        loadMusicLibrary();
    }
}

// ==================== 音乐库功能 ====================
async function loadMusicLibrary() {
    if (isLibraryLoading) return;

    isLibraryLoading = true;
    const musicLibraryEl = document.getElementById('musicLibrary');
    const refreshBtn = document.getElementById('refreshLibraryBtn');
    const originalContent = refreshBtn.innerHTML;

    // 显示加载状态
    refreshBtn.disabled = true;
    refreshBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">refresh</span> 加载中...';
    musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block; animation: spin 1s linear infinite;">refresh</span><div style="font-size: 16px; font-weight: 500;">加载音乐库中...</div></div>';

    try {
        const response = await fetch('/api/library/list');
        if (response.ok) {
            musicLibrary = await response.json();
            renderMusicLibrary();
            showToast('音乐库加载成功', 'success');
        } else {
            const errorData = await response.json();
            showToast('加载失败: ' + (errorData.error || '未知错误'), 'warning');
            musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">error</span><div style="font-size: 16px; font-weight: 500;">加载失败</div><div style="font-size: 14px; margin-top: 8px;">' + (errorData.error || '请重试') + '</div></div>';
        }
    } catch (error) {
        showToast('加载失败: ' + error.message, 'warning');
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">wifi_off</span><div style="font-size: 16px; font-weight: 500;">网络连接失败</div><div style="font-size: 14px; margin-top: 8px;">请检查服务器是否运行</div></div>';
    } finally {
        isLibraryLoading = false;
        refreshBtn.disabled = false;
        refreshBtn.innerHTML = originalContent;
    }
}

function renderMusicLibrary() {
    const musicLibraryEl = document.getElementById('musicLibrary');
    const musicCountEl = document.getElementById('musicCount');

    if (!musicLibrary || musicLibrary.length === 0) {
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">library_music</span><div style="font-size: 16px; font-weight: 500;">音乐库为空</div><div style="font-size: 14px; margin-top: 8px;">前往下载页面添加音乐</div></div>';
        musicCountEl.textContent = '0';
        return;
    }

    musicCountEl.textContent = musicLibrary.length;

    let html = '<div class="music-list-header"><span></span><span>歌曲</span><span class="col-center">延迟</span><span class="col-center">大小</span><span class="col-center">下载时间</span><span></span></div>';

    musicLibrary.forEach((music, index) => {
        const customFilename = music.custom_filename || '未知文件';
        const systemFilename = music.system_filename || customFilename;
        const fileSize = music.file_size || 0;
        const downloadTime = music.download_time ? new Date(music.download_time * 1000) : new Date();
        const delayMs = music.delay_ms || 0;

        const sizeText = formatBytes(fileSize);
        const timeText = formatDateTime(downloadTime);
        const title = customFilename.replace(/\.[^/.]+$/, '') || '未知歌曲';
        const indexNum = String(index + 1).padStart(2, '0');

        html += `
        <div class="music-item" data-index="${index}">
            <div class="music-index">
                <span class="music-index-num">${indexNum}</span>
                <button class="music-index-play" title="播放" onclick="playMusicFromLibrary('${systemFilename}')">
                    <span class="material-symbols-rounded">play_arrow</span>
                </button>
            </div>
            <div class="music-name">
                <div class="music-title" title="${title}">${title}</div>
                <div class="music-subtitle">${delayMs}ms · ${sizeText}</div>
            </div>
            <div class="music-delay">${delayMs}ms</div>
            <div class="music-size">${sizeText}</div>
            <div class="music-time">${timeText}</div>
            <div class="music-actions-cell">
                <button class="music-action-btn" title="下载" onclick="downloadMusicFile('${systemFilename}')">
                    <span class="material-symbols-rounded">download</span>
                </button>
            </div>
        </div>
        `;
    });

    musicLibraryEl.innerHTML = html;
}

function formatDownloadTime(date) {
    const now = new Date();
    const diffMs = now - date;
    const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));

    if (diffDays === 0) {
        return '今天';
    } else if (diffDays === 1) {
        return '昨天';
    } else if (diffDays < 7) {
        return `${diffDays}天前`;
    } else if (diffDays < 30) {
        return `${Math.floor(diffDays / 7)}周前`;
    } else {
        return date.toLocaleDateString('zh-CN', { year: 'numeric', month: 'short', day: 'numeric' });
    }
}

function formatDateTime(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return `${year}-${month}-${day} ${hours}:${minutes}`;
}

async function downloadMusicFile(systemFilename) {
    if (!systemFilename) {
        showToast('文件名无效', 'warning');
        return;
    }

    showToast('准备下载文件...', 'info');

    try {
        // 使用系统文件名作为参数
        const response = await fetch(`/api/download/file?filename=${encodeURIComponent(systemFilename)}`);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let downloadFilename = systemFilename;

            // 尝试获取自定义文件名作为下载时的显示文件名
            const musicItem = musicLibrary.find(item => item.system_filename === systemFilename);
            if (musicItem && musicItem.custom_filename) {
                downloadFilename = musicItem.custom_filename;
            }

            if (contentDisposition) {
                const matches = /filename="?([^"]+)"?/i.exec(contentDisposition);
                if (matches && matches[1]) downloadFilename = matches[1];
            }
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = downloadFilename;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
            showToast('文件开始下载到您的设备', 'success');
        } else {
            const errorData = await response.json();
            showToast('下载未完成: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('下载错误: ' + error.message, 'warning');
    }
}

async function playMusicFromLibrary(systemFilename) {
    if (!systemFilename) {
        showToast('文件名无效', 'warning');
        return;
    }

    console.log('尝试播放文件:', systemFilename);
    console.log('当前音乐库:', musicLibrary);

    // 在音乐库中查找对应的文件
    const musicItem = musicLibrary.find(item => item.system_filename === systemFilename);
    if (!musicItem) {
        console.log('未找到对应的音乐文件，尝试查找自定义文件名');
        // 尝试通过自定义文件名查找
        const altMusicItem = musicLibrary.find(item => item.custom_filename === systemFilename);
        if (!altMusicItem) {
            showToast('未找到对应的音乐文件', 'warning');
            return;
        }
        // 使用找到的项
        console.log('通过自定义文件名找到文件:', altMusicItem);
    }

    // 确保音频播放器已初始化
    if (!audioPlayer) {
        initAudioPlayer();
    }

    // 设置音频播放器
    const streamUrl = `/api/download/stream?filename=${encodeURIComponent(systemFilename)}&t=${Date.now()}`;
    console.log('播放URL:', streamUrl);

    if (audioPlayer) {
        audioPlayer.src = streamUrl;
        audioPlayer.load();

        // 显示音频播放器
        if (audioSection) {
            // 确保弹窗在可视区域内
            ensurePopupInViewport();
            audioSection.classList.add('show');
        }

        // 更新播放器信息
        const customFilename = musicItem ? musicItem.custom_filename : systemFilename;
        const title = customFilename.replace(/\.[^/.]+$/, '') || '未知歌曲';
        if (playerTitle) {
            playerTitle.textContent = title;
        }
        if (playerArtist) {
            playerArtist.textContent = '从音乐库播放';
        }

        // 加载歌词
        try {
            console.log('[歌词调试] === 开始歌词加载流程 ===');
            showToast('正在加载歌词...', 'info');
            const lyricsText = await fetchLyrics(systemFilename);
            
            if (lyricsText) {
                console.log('[歌词调试] 获取到歌词文本，开始解析');
                currentLyrics = parseLrcLyrics(lyricsText);
                console.log('[歌词调试] 歌词解析完成，开始显示');
                displayLyrics(currentLyrics);
                showToast('歌词加载成功', 'success');
                console.log('[歌词调试] === 歌词加载成功 ===');
            } else {
                console.log('[歌词调试] 未获取到歌词文本');
                currentLyrics = [];
                displayLyrics([]);
                showToast('未找到歌词', 'info');
                console.log('[歌词调试] === 未找到歌词 ===');
            }
        } catch (error) {
            console.error('[歌词调试] === 歌词加载失败 ===');
            console.error('[歌词调试] 错误对象:', error);
            console.error('[歌词调试] 错误消息:', error.message || error);
            console.error('[歌词调试] 错误堆栈:', error.stack || '无堆栈信息');
            
            // 显示更详细的错误信息给用户
            let errorMessage = '歌词加载失败';
            const errorMsg = error.message || String(error);
            if (errorMsg.includes('jsmediatags库未加载')) {
                errorMessage = '歌词库加载失败，请刷新页面';
            } else if (errorMsg.includes('HTTP')) {
                errorMessage = '服务器请求失败: ' + errorMsg;
            } else if (errorMsg.includes('Blob')) {
                errorMessage = '音频文件读取失败';
            } else if (errorMsg.includes('Offset') && errorMsg.includes('hasn\'t been loaded yet')) {
                errorMessage = '歌词数据不完整，尝试重新加载';
            }
            
            currentLyrics = [];
            displayLyrics([]);
            showToast(errorMessage, 'warning');
            console.log('[歌词调试] === 错误处理完成 ===');
        }

        // 自动播放
        setTimeout(() => {
            audioPlayer.play().catch(e => {
                console.log('自动播放被阻止:', e);
                showToast('点击播放按钮开始播放', 'info');
            });
        }, 500);

        showToast('正在加载音乐...', 'info');
    } else {
        showToast('音频播放器初始化失败', 'warning');
    }
}

urlInput.addEventListener('keydown', (e) => { if (e.key === 'Enter' && e.ctrlKey) { e.preventDefault(); sendBtn.click(); } });
filenameInput.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); sendBtn.click(); } });
window.addEventListener('beforeunload', () => stopPolling());

// 全局变量
let pollingInterval = null;
let currentTaskId = null;
let lastSpeedUpdateTime = Date.now();
let lastSpeedBytes = 0;
let speedHistory = [];
const MAX_SPEED_HISTORY = 3;
let downloadStartTime = null;
let isScrolling = false;
const processedErrors = new Set();

// === 音频播放控制变量（增强）===
let isSeeking = false;
let audioPlaybackBufferTimer = null;
let audioLoadTimer = null; // 新增：音频加载超时控制
const AUDIO_LOAD_TIMEOUT = 10000; // 10秒加载超时

document.addEventListener('DOMContentLoaded', function() {
    const audioPlayer = document.getElementById('audioPlayer');
    
    // === 核心修复1：设置音频预加载策略 ===
    audioPlayer.preload = 'auto'; // 关键：让浏览器主动缓冲音频
    
    // === 核心修复2：监听音频可播放状态 ===
    audioPlayer.addEventListener('canplaythrough', function() {
        console.log('音频缓冲充足，可流畅播放');
        // 清除加载超时定时器
        if (audioLoadTimer) {
            clearTimeout(audioLoadTimer);
            audioLoadTimer = null;
        }
    });
    
    // === 核心修复3：监听音频元数据加载 ===
    audioPlayer.addEventListener('loadedmetadata', function() {
        console.log('音频元数据加载完成，时长:', audioPlayer.duration);
    });

    // === 优化：拖动进度条处理 ===
    audioPlayer.addEventListener('seeking', function() {
        console.log('开始拖动进度条');
        isSeeking = true;
        
        if (audioPlaybackBufferTimer) {
            clearTimeout(audioPlaybackBufferTimer);
            audioPlaybackBufferTimer = null;
        }
    });

    audioPlayer.addEventListener('seeked', function() {
        console.log('拖动进度条完成');
        
        // 延迟确认拖动结束，避免误触发
        setTimeout(() => {
            isSeeking = false;
            
            // 如果音频暂停但未结束，尝试恢复
        }, 150);
    });

    // === 关键修复4：优化卡顿和等待事件处理 ===
    audioPlayer.addEventListener('stalled', function() {
        console.log('音频数据卡顿');
        // 不立即干预，给浏览器2秒自行恢复
    });

    audioPlayer.addEventListener('waiting', function() {
        console.log('音频等待数据加载...');
        // 仅在非拖动状态下处理
    });

    // === 核心修复5：增强错误处理，避免过度反应 ===
    audioPlayer.addEventListener('error', async function(e) {
        // 如果是拖动导致的错误，延迟处理
        if (isSeeking) {
            console.log('拖动中发生错误，稍后重试');
            setTimeout(() => handleAudioError(), 300);
            return;
        }
        
        await handleAudioError();
    });
    
    // 独立的错误处理函数
    async function handleAudioError() {
        console.error('音频错误详情:', audioPlayer.error);
    }

    // === 新增：监听播放结束 ===
    audioPlayer.addEventListener('ended', function() {
        console.log('音频播放完成');
        isSeeking = false;
    });

    // === 新增：监听播放开始 ===
    audioPlayer.addEventListener('play', function() {
        console.log('音频开始播放，当前状态:', audioPlayer.readyState);
    });

    // 主题管理（保持不变）
    const themeToggle = document.getElementById('themeToggle');
    const paletteToggle = document.getElementById('paletteToggle');
    const themeSelector = document.getElementById('themeSelector');
    const themeColors = document.querySelectorAll('.theme-color');
    const root = document.documentElement;
    
    let currentTheme = localStorage.getItem('theme') || 'default';
    let isDarkMode = localStorage.getItem('darkMode') === 'true';
    
    function initTheme() {
        root.setAttribute('data-theme', isDarkMode ? 'dark' : currentTheme);
        themeToggle.textContent = isDarkMode ? '☀️' : '🌙';
        
        themeColors.forEach(color => {
            color.classList.toggle('active', color.dataset.theme === currentTheme);
        });
    }
    
    themeToggle.addEventListener('click', () => {
        isDarkMode = !isDarkMode;
        localStorage.setItem('darkMode', isDarkMode);
        initTheme();
    });
    
    paletteToggle.addEventListener('click', (e) => {
        e.stopPropagation();
        themeSelector.classList.toggle('show');
    });
    
    themeColors.forEach(color => {
        color.addEventListener('click', (e) => {
            e.stopPropagation();
            currentTheme = color.dataset.theme;
            localStorage.setItem('theme', currentTheme);
            isDarkMode = false;
            localStorage.setItem('darkMode', false);
            initTheme();
            themeSelector.classList.remove('show');
        });
    });
    
    document.addEventListener('click', () => {
        themeSelector.classList.remove('show');
    });
    
    initTheme();
    
    // 音乐下载功能（保持不变）
    const urlInput = document.getElementById('urlInput');
    const filenameInput = document.getElementById('filenameInput');
    const sendBtn = document.getElementById('sendBtn');
    const localDownloadBtn = document.getElementById('localDownloadBtn');
    const statusMessage = document.getElementById('statusMessage');
    const fileInfo = document.getElementById('fileInfo');
    
    const statusLight = document.getElementById('statusLight');
    const downloadedBytesEl = document.getElementById('downloadedBytes');
    const downloadSpeedEl = document.getElementById('downloadSpeed');
    const taskStatusEl = document.getElementById('taskStatus');
    const fileNameEl = document.getElementById('fileName');
    const fileSizeEl = document.getElementById('fileSize');
    const downloadTimeEl = document.getElementById('downloadTime');
    const downloadStatusEl = document.getElementById('downloadStatus');
    
    function updateStatusLight(status) {
        statusLight.className = 'light-small';
        
        switch(status) {
            case 'error':
                statusLight.classList.add('red');
                taskStatusEl.textContent = '下载失败';
                downloadStatusEl.textContent = '失败';
                break;
            case 'downloading':
                statusLight.classList.add('yellow');
                taskStatusEl.textContent = '下载中';
                downloadStatusEl.textContent = '进行中';
                break;
            case 'success':
                statusLight.classList.add('green');
                taskStatusEl.textContent = '下载完成';
                downloadStatusEl.textContent = '已完成';
                break;
            case 'idle':
            default:
                taskStatusEl.textContent = '等待中';
                downloadStatusEl.textContent = '-';
                break;
        }
    }
    
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
    
    function showStatus(message, type) {
        statusMessage.textContent = message;
        statusMessage.className = `status-message ${type}`;
        statusMessage.style.display = 'block';
        
        setTimeout(() => {
            statusMessage.style.display = 'none';
        }, 4000);
    }
    
    function startPollingTaskStatus() {
        if (pollingInterval) {
            clearInterval(pollingInterval);
        }
        
        downloadStartTime = Date.now();
        
        pollingInterval = setInterval(async () => {
            if (!currentTaskId) {
                stopPolling();
                return;
            }
            
            try {
                const response = await fetch(`/api/download/status?task_id=${currentTaskId}`, {
                    method: 'GET',
                    headers: {
                        'Content-Type': 'application/json',
                        'Cache-Control': 'no-cache'
                    }
                });
                
                if (response.ok) {
                    const status = await response.json();
                    updateTaskStatus(status);
                } else if (response.status === 404) {
                    showStatus('任务不存在或已过期', 'error');
                    updateStatusLight('error');
                    stopPolling();
                }
            } catch (error) {
                console.error('轮询错误:', error);
            }
        }, 1000);
    }
    
    function updateTaskStatus(status) {
        if (status.error) {
            taskStatusEl.textContent = '错误: ' + status.error;
            updateStatusLight('error');
            stopPolling();
            return;
        }
        
        if (status.is_downloading) {
            updateStatusLight('downloading');
            localDownloadBtn.style.display = 'none';
            
        } else if (status.is_finished) {
            if (status.is_success) {
                updateStatusLight('success');
                localDownloadBtn.style.display = 'block';
                localDownloadBtn.disabled = false;
                
                // === 核心修复6：优化音频源设置时机和方式 ===
                if (currentTaskId && !window.audioLoadedFlag) {
                    window.audioLoadedFlag = true;  // 全局标志，防止重复
                    
                    setTimeout(() => {
                        audioPlayer.src = `/api/download/stream?task_id=${currentTaskId}&t=${Date.now()}`;
                        audioPlayer.load();  // 第 1 次加载
                        
                        // 超时保护，只超时再加载一次
                        audioLoadTimer = setTimeout(() => {
                            if (audioPlayer.networkState === 2) {
                                audioPlayer.load();  // 第 2 次加载（仅超时）
                            }
                        }, AUDIO_LOAD_TIMEOUT);
                    }, 1000);
                }
                
                if (status.file_info) {
                    fileNameEl.textContent = status.file_info.filename || '未命名文件';
                    fileSizeEl.textContent = formatBytes(status.file_info.filesize || 0);
                    
                    if (downloadStartTime) {
                        const downloadTime = (Date.now() - downloadStartTime) / 1000;
                        downloadTimeEl.textContent = formatTime(downloadTime);
                    }
                    
                    fileInfo.style.display = 'block';
                }
                
                showStatus('🎉 文件下载完成！可以保存到本地了', 'success');
            } else {
                updateStatusLight('error');
                localDownloadBtn.style.display = 'none';
            }
            
            setTimeout(() => {
                stopPolling();
            }, 3000);
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
            if (speedHistory.length > MAX_SPEED_HISTORY) {
                speedHistory.shift();
            }
            
            const avgSpeed = speedHistory.reduce((sum, val) => sum + val, 0) / speedHistory.length;
            downloadSpeedEl.textContent = formatSpeed(avgSpeed);
            
            lastSpeedBytes = downloadedBytes;
            lastSpeedUpdateTime = now;
        }
    }
    
    function stopPolling() {
        if (pollingInterval) {
            clearInterval(pollingInterval);
            pollingInterval = null;
        }
    }
    
    function resetTaskStatus() {
        downloadedBytesEl.textContent = '0 B';
        downloadSpeedEl.textContent = '0 B/s';
        taskStatusEl.textContent = '等待中';
        downloadTimeEl.textContent = '-';
        fileInfo.style.display = 'none';
        
        // 清理音频相关定时器
        if (audioLoadTimer) {
            clearTimeout(audioLoadTimer);
            audioLoadTimer = null;
        }
        if (audioPlaybackBufferTimer) {
            clearTimeout(audioPlaybackBufferTimer);
            audioPlaybackBufferTimer = null;
        }
        
        // 温和地重置音频播放器
        audioPlayer.pause();
        audioPlayer.src = '';
        audioPlayer.removeAttribute('src');
        audioPlayer.load();
        
        updateStatusLight('idle');
        localDownloadBtn.style.display = 'none';
        localDownloadBtn.disabled = true;
        
        lastSpeedBytes = 0;
        lastSpeedUpdateTime = Date.now();
        speedHistory = [];
        downloadStartTime = null;
        window.audioLoadedFlag = false;
    }
    
    sendBtn.addEventListener('click', async function() {
        const inputText = urlInput.value.trim();
        
        if (!inputText) {
            showStatus('请输入内容！', 'error');
            urlInput.focus();
            return;
        }
        
        const url = extractUrlFromText(inputText);
        if (!url) {
            showStatus('未找到有效的链接！', 'error');
            urlInput.focus();
            return;
        }
        
        // ====== 新增：获取音乐平台和delay参数（优化版） ======
        const platformSelect = document.getElementById('platformSelect');
        const platform = platformSelect ? platformSelect.value : "酷狗音乐"; // 默认值
        
        const delayInput = document.getElementById('delayInput');
        let delayValue = 0; // 默认值
    
        if (delayInput) {
            const delayText = delayInput.value.trim();
            if (delayText !== '') {
                const parsedDelay = parseInt(delayText);
                if (isNaN(parsedDelay)) {
                    showStatus('Delay参数必须是整数！', 'error');
                    delayInput.focus();
                    delayInput.select();
                    return;
                }
                delayValue = parsedDelay;
            }
            // 如果为空字符串，保持默认值0
        }
        // ====== 新增结束 ======
        
        resetTaskStatus();
        showStatus('正在创建下载任务...', 'info');
        
        sendBtn.disabled = true;
        sendBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 发送中...';
        
        try {
            const requestData = {
                content: inputText,
                url: url,
                // ====== 新增：发送音乐平台和delay参数 ======
                platform: platform,
                offsetMs: delayValue
                // ====== 新增结束 ======
            };
            
            const filename = filenameInput.value.trim();
            if (filename) {
                requestData.filename = filename;
            }
            
            // 调试信息：查看发送的数据
            console.log('发送到后端的数据:', requestData);
            
            const response = await fetch('/api/message', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(requestData)
            });
            
            const data = await response.json();
            
            if (response.ok) {
                currentTaskId = data.task_id;
                showStatus('下载任务已创建，开始下载...', 'success');
                updateStatusLight('downloading');
                
                setTimeout(() => {
                    startPollingTaskStatus();
                }, 500);
            } else {
                showStatus(`下载失败: ${data.error || '未知错误'}`, 'error');
                updateStatusLight('error');
            }
        } catch (error) {
            showStatus(`连接错误: ${error.message}`, 'error');
            updateStatusLight('error');
        } finally {
            sendBtn.disabled = false;
            sendBtn.innerHTML = '<i class="fas fa-download"></i> 开始下载';
        }
    });
    
    localDownloadBtn.addEventListener('click', async function() {
        if (!currentTaskId) {
            showStatus('没有可下载的文件', 'error');
            return;
        }
        
        showStatus('正在准备文件...', 'info');
        localDownloadBtn.disabled = true;
        localDownloadBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 准备中...';
        
        try {
            const response = await fetch(`/api/download/file?task_id=${currentTaskId}`, {
                method: 'GET'
            });
            
            if (response.ok) {
                const contentDisposition = response.headers.get('Content-Disposition');
                let filename = 'downloaded_music';
                
                if (contentDisposition) {
                    const matches = /filename="?([^"]+)"?/i.exec(contentDisposition);
                    if (matches && matches[1]) {
                        filename = matches[1];
                    }
                }
                
                const blob = await response.blob();
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                
                showStatus('文件开始下载到您的设备', 'success');
            } else {
                const errorData = await response.json();
                showStatus(`下载失败: ${errorData.error || '未知错误'}`, 'error');
            }
        } catch (error) {
            showStatus(`下载错误: ${error.message}`, 'error');
        } finally {
            localDownloadBtn.disabled = false;
            localDownloadBtn.innerHTML = '<i class="fas fa-file-download"></i> 保存到本地';
        }
    });
    
    setTimeout(() => {
        showStatus('🎵 输入链接或包含链接的文本即可开始下载', 'info');
    }, 1000);
    
    window.addEventListener('beforeunload', function() {
        stopPolling();
    });
    
    urlInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter' && e.ctrlKey) {
            e.preventDefault();
            sendBtn.click();
        }
    });
    
    filenameInput.addEventListener('keydown', function(e) {
        if (e.key === 'Enter') {
            e.preventDefault();
            sendBtn.click();
        }
    });
    
    const inputs = document.querySelectorAll('input, textarea');
    inputs.forEach(input => {
        input.addEventListener('focus', () => {
            input.parentElement.style.transform = 'translateY(-5px)';
        });
        
        input.addEventListener('blur', () => {
            input.parentElement.style.transform = 'translateY(0)';
        });
    });
    
    function scrollToTop() {
        if (isScrolling) return;
        isScrolling = true;
        
        if ('scrollBehavior' in document.documentElement.style) {
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
            
            setTimeout(() => {
                isScrolling = false;
            }, 500);
        } else {
            const duration = 600;
            const start = window.pageYOffset;
            const startTime = performance.now();
            
            function animateScroll(currentTime) {
                const elapsed = currentTime - startTime;
                const progress = Math.min(elapsed / duration, 1);
                
                const easeInOutCubic = t => t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
                const easedProgress = easeInOutCubic(progress);
                
                window.scrollTo(0, start * (1 - easedProgress));
                
                if (progress < 1) {
                    requestAnimationFrame(animateScroll);
                } else {
                    isScrolling = false;
                }
            }
            
            requestAnimationFrame(animateScroll);
        }
    }
    
    const scrollTopBtn = document.getElementById('scrollTop');
    
    if (scrollTopBtn) {
        scrollTopBtn.addEventListener('click', function(e) {
            e.preventDefault();
            e.stopPropagation();
            scrollToTop();
        });
        
        scrollTopBtn.addEventListener('touchstart', function(e) {
            e.preventDefault();
        }, { passive: false });
        
        scrollTopBtn.addEventListener('touchend', function(e) {
            e.preventDefault();
            scrollToTop();
        }, { passive: false });
        
        scrollTopBtn.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                scrollToTop();
            }
        });
        
        scrollTopBtn.setAttribute('tabindex', '0');
        scrollTopBtn.setAttribute('role', 'button');
        scrollTopBtn.setAttribute('aria-label', '回到页面顶部');
    } else {
        console.error('未找到回到顶部按钮元素');
    }
    
    if ('visualViewport' in window) {
        const onResize = () => {
            const viewport = window.visualViewport;
            if (viewport.height < window.innerHeight) {
                document.body.style.height = viewport.height + 'px';
            } else {
                document.body.style.height = '100vh';
            }
        };
        
        window.visualViewport.addEventListener('resize', onResize);
        window.visualViewport.addEventListener('scroll', onResize);
    }
    
    let lastScrollTop = 0;
    window.addEventListener('scroll', function() {
        const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
        const scrollTopBtn = document.getElementById('scrollTop');
        
        if (scrollTopBtn) {
            if (scrollTop > 200) {
                scrollTopBtn.style.opacity = '1';
                scrollTopBtn.style.visibility = 'visible';
            } else {
                scrollTopBtn.style.opacity = '0';
                scrollTopBtn.style.visibility = 'hidden';
            }
        }
        
        lastScrollTop = scrollTop;
    });
    
    urlInput.focus();
});
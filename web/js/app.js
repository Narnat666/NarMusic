// 全局变量
let pollingInterval = null;
let currentTaskId = null;
let lastSpeedUpdateTime = Date.now();
let lastSpeedBytes = 0;
let speedHistory = [];
const MAX_SPEED_HISTORY = 3;
let downloadStartTime = null;
let isScrolling = false; // 添加滚动状态控制

document.addEventListener('DOMContentLoaded', function() {
    // 主题管理
    const themeToggle = document.getElementById('themeToggle');
    const paletteToggle = document.getElementById('paletteToggle');
    const themeSelector = document.getElementById('themeSelector');
    const themeColors = document.querySelectorAll('.theme-color');
    const root = document.documentElement;
    
    let currentTheme = localStorage.getItem('theme') || 'default';
    let isDarkMode = localStorage.getItem('darkMode') === 'true';
    
    // 初始化主题
    function initTheme() {
        root.setAttribute('data-theme', isDarkMode ? 'dark' : currentTheme);
        themeToggle.textContent = isDarkMode ? '☀️' : '🌙';
        
        // 激活当前主题颜色
        themeColors.forEach(color => {
            color.classList.toggle('active', color.dataset.theme === currentTheme);
        });
    }
    
    // 切换暗黑/明亮模式
    themeToggle.addEventListener('click', () => {
        isDarkMode = !isDarkMode;
        localStorage.setItem('darkMode', isDarkMode);
        initTheme();
    });
    
    // 显示/隐藏调色板
    paletteToggle.addEventListener('click', (e) => {
        e.stopPropagation();
        themeSelector.classList.toggle('show');
    });
    
    // 选择主题颜色
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
    
    // 点击其他地方关闭调色板
    document.addEventListener('click', () => {
        themeSelector.classList.remove('show');
    });
    
    // 初始化主题
    initTheme();
    
    // 音乐下载功能
    const urlInput = document.getElementById('urlInput');
    const filenameInput = document.getElementById('filenameInput');
    const sendBtn = document.getElementById('sendBtn');
    const clearBtn = document.getElementById('clearBtn');
    const localDownloadBtn = document.getElementById('localDownloadBtn');
    const statusMessage = document.getElementById('statusMessage');
    const fileInfo = document.getElementById('fileInfo');
    
    // 状态相关元素
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
    
    // 格式化字节大小
    function formatBytes(bytes) {
        if (bytes === 0 || bytes === undefined) return '0 B';
        
        const k = 1024;
        const sizes = ['B', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        
        if (i === 0) return bytes + ' ' + sizes[i];
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
    
    // 格式化速度
    function formatSpeed(bytesPerSecond) {
        if (bytesPerSecond === 0) return '0 B/s';
        
        const k = 1024;
        const sizes = ['B/s', 'KB/s', 'MB/s'];
        const i = Math.floor(Math.log(bytesPerSecond) / Math.log(k));
        
        if (i >= sizes.length) i = sizes.length - 1;
        if (i === 0) return bytesPerSecond + ' ' + sizes[i];
        return parseFloat((bytesPerSecond / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }
    
    // 格式化时间
    function formatTime(seconds) {
        if (seconds < 60) return seconds.toFixed(1) + '秒';
        if (seconds < 3600) return Math.floor(seconds / 60) + '分' + Math.floor(seconds % 60) + '秒';
        return Math.floor(seconds / 3600) + '小时' + Math.floor((seconds % 3600) / 60) + '分';
    }
    
    // 从文本中提取URL
    function extractUrlFromText(text) {
        // 尝试匹配URL模式
        const urlPattern = /(https?:\/\/[^\s]+|www\.[^\s]+|[^\s]+\.[a-z]{2,}\/[^\s]*)/gi;
        const matches = text.match(urlPattern);
        
        if (matches && matches.length > 0) {
            let url = matches[0];
            
            // 如果URL没有协议，添加https://
            if (!url.startsWith('http://') && !url.startsWith('https://')) {
                url = 'https://' + url;
            }
            
            return url;
        }
        
        return null;
    }
    
    // 显示状态消息
    function showStatus(message, type) {
        statusMessage.textContent = message;
        statusMessage.className = `status-message ${type}`;
        statusMessage.style.display = 'block';
        
        setTimeout(() => {
            statusMessage.style.display = 'none';
        }, 4000);
    }
    
    // 开始轮询下载状态
    function startPollingTaskStatus() {
        if (pollingInterval) {
            clearInterval(pollingInterval);
        }
        
        // 记录下载开始时间
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
    
    // 更新任务状态
    function updateTaskStatus(status) {
        // 处理错误状态
        if (status.error) {
            taskStatusEl.textContent = '错误: ' + status.error;
            updateStatusLight('error');
            stopPolling();
            return;
        }
        
        // 更新任务状态
        if (status.is_downloading) {
            updateStatusLight('downloading');
            localDownloadBtn.style.display = 'none';
            
        } else if (status.is_finished) {
            if (status.is_success) {
                updateStatusLight('success');
                
                // 显示下载本地按钮
                localDownloadBtn.style.display = 'block';
                localDownloadBtn.disabled = false;
                
                // 更新文件信息
                if (status.file_info) {
                    fileNameEl.textContent = status.file_info.filename || '未命名文件';
                    fileSizeEl.textContent = formatBytes(status.file_info.filesize || 0);
                    
                    // 计算下载时间
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
            
            // 下载完成后停止轮询
            setTimeout(() => {
                stopPolling();
            }, 3000);
        } else {
            updateStatusLight('idle');
            localDownloadBtn.style.display = 'none';
        }
        
        // 更新已下载字节数
        const downloadedBytes = status.downloaded_bytes || 0;
        downloadedBytesEl.textContent = formatBytes(downloadedBytes);
        
        // 计算下载速度
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
    
    // 停止轮询
    function stopPolling() {
        if (pollingInterval) {
            clearInterval(pollingInterval);
            pollingInterval = null;
        }
    }
    
    // 重置任务状态显示
    function resetTaskStatus() {
        downloadedBytesEl.textContent = '0 B';
        downloadSpeedEl.textContent = '0 B/s';
        taskStatusEl.textContent = '等待中';
        downloadTimeEl.textContent = '-';
        fileInfo.style.display = 'none';
        
        updateStatusLight('idle');
        localDownloadBtn.style.display = 'none';
        localDownloadBtn.disabled = true;
        
        lastSpeedBytes = 0;
        lastSpeedUpdateTime = Date.now();
        speedHistory = [];
        downloadStartTime = null;
    }
    
    // 发送下载请求
    sendBtn.addEventListener('click', async function() {
        const inputText = urlInput.value.trim();
        
        if (!inputText) {
            showStatus('请输入内容！', 'error');
            urlInput.focus();
            return;
        }
        
        // 从输入文本中提取URL
        const url = extractUrlFromText(inputText);
        if (!url) {
            showStatus('未找到有效的链接！', 'error');
            urlInput.focus();
            return;
        }
        
        // 重置状态
        resetTaskStatus();
        showStatus('正在创建下载任务...', 'info');
        
        sendBtn.disabled = true;
        sendBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> 发送中...';
        
        try {
            // 准备请求数据
            const requestData = {
                content: inputText,  // 发送原始内容，让后端处理
                url: url            // 也发送提取的URL
            };
            
            // 如果用户输入了文件名，添加到请求中
            const filename = filenameInput.value.trim();
            if (filename) {
                requestData.filename = filename;
            }
            
            const response = await fetch('/api/message', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(requestData)
            });
            
            const data = await response.json();
            
            if (response.ok) {
                // 保存任务ID
                currentTaskId = data.task_id;
                
                showStatus('下载任务已创建，开始下载...', 'success');
                
                // 更新状态
                updateStatusLight('downloading');
                
                // 开始轮询下载状态
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
    
    // 清空内容
    clearBtn.addEventListener('click', function() {
        urlInput.value = '';
        filenameInput.value = '';
        statusMessage.style.display = 'none';
        urlInput.focus();
        stopPolling();
        resetTaskStatus();
        currentTaskId = null;
        showStatus('已清空所有输入和状态', 'info');
    });
    
    // 下载本地文件
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
                // 获取文件名
                const contentDisposition = response.headers.get('Content-Disposition');
                let filename = 'downloaded_music';
                
                if (contentDisposition) {
                    const matches = /filename="?([^"]+)"?/i.exec(contentDisposition);
                    if (matches && matches[1]) {
                        filename = matches[1];
                    }
                }
                
                // 创建下载链接
                const blob = await response.blob();
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = filename;
                document.body.appendChild(a);
                a.click();
                
                // 清理
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
    
    // 页面加载完成后显示欢迎信息
    setTimeout(() => {
        showStatus('🎵 输入链接或包含链接的文本即可开始下载', 'info');
    }, 1000);
    
    // 页面卸载时停止轮询
    window.addEventListener('beforeunload', function() {
        stopPolling();
    });
    
    // 添加键盘快捷键
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
    
    // 添加输入框动画效果
    const inputs = document.querySelectorAll('input, textarea');
    inputs.forEach(input => {
        input.addEventListener('focus', () => {
            input.parentElement.style.transform = 'translateY(-5px)';
        });
        
        input.addEventListener('blur', () => {
            input.parentElement.style.transform = 'translateY(0)';
        });
    });
    
    // 修复：确保页面滚动功能正常
    function scrollToTop() {
        console.log('开始滚动到顶部...');
        
        // 如果已经在滚动中，不要重复触发
        if (isScrolling) return;
        isScrolling = true;
        
        // 方法1：使用现代浏览器的平滑滚动
        if ('scrollBehavior' in document.documentElement.style) {
            window.scrollTo({
                top: 0,
                behavior: 'smooth'
            });
            
            // 设置定时器重置滚动状态
            setTimeout(() => {
                isScrolling = false;
                console.log('平滑滚动完成');
            }, 500);
        } 
        // 方法2：回退方案 - 手动实现平滑滚动
        else {
            const duration = 600;
            const start = window.pageYOffset;
            const startTime = performance.now();
            
            function animateScroll(currentTime) {
                const elapsed = currentTime - startTime;
                const progress = Math.min(elapsed / duration, 1);
                
                // 使用缓动函数使滚动更平滑
                const easeInOutCubic = t => t < 0.5 ? 4 * t * t * t : (t - 1) * (2 * t - 2) * (2 * t - 2) + 1;
                const easedProgress = easeInOutCubic(progress);
                
                window.scrollTo(0, start * (1 - easedProgress));
                
                if (progress < 1) {
                    requestAnimationFrame(animateScroll);
                } else {
                    isScrolling = false;
                    console.log('手动滚动完成');
                }
            }
            
            requestAnimationFrame(animateScroll);
        }
    }
    
    // 修复：回到顶部功能 - 增强版本
    const scrollTopBtn = document.getElementById('scrollTop');
    
    if (scrollTopBtn) {
        console.log('找到回到顶部按钮:', scrollTopBtn);
        
        // 添加点击事件
        scrollTopBtn.addEventListener('click', function(e) {
            console.log('回到顶部按钮被点击');
            e.preventDefault();
            e.stopPropagation();
            scrollToTop();
        });
        
        // 添加触摸事件支持（移动端）
        scrollTopBtn.addEventListener('touchstart', function(e) {
            e.preventDefault();
        }, { passive: false });
        
        scrollTopBtn.addEventListener('touchend', function(e) {
            e.preventDefault();
            scrollToTop();
        }, { passive: false });
        
        // 添加键盘支持（无障碍）
        scrollTopBtn.addEventListener('keydown', function(e) {
            if (e.key === 'Enter' || e.key === ' ') {
                e.preventDefault();
                scrollToTop();
            }
        });
        
        // 确保按钮可聚焦
        scrollTopBtn.setAttribute('tabindex', '0');
        scrollTopBtn.setAttribute('role', 'button');
        scrollTopBtn.setAttribute('aria-label', '回到页面顶部');
        
        console.log('回到顶部按钮初始化完成');
    } else {
        console.error('未找到回到顶部按钮元素');
    }
    
    // 全屏模式优化：防止键盘遮挡输入框
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
    
    // 添加滚动监听，控制回到顶部按钮的显示
    let lastScrollTop = 0;
    window.addEventListener('scroll', function() {
        const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
        const scrollTopBtn = document.getElementById('scrollTop');
        
        if (scrollTopBtn) {
            // 如果向下滚动超过200px，显示按钮
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
    
    // 自动聚焦到输入框
    urlInput.focus();
});
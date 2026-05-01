import { api } from '../api.js';
import { formatBytes, formatSpeed, formatTime, extractUrlFromText, showToast, parseFilenameFromContentDisposition, triggerDownload, streamDownloadWithProgress } from '../utils.js';
import { getSettingsValues } from './settings.js';

let pollingInterval = null;
let currentTaskId = null;
let isLocalDownloading = false;
let lastSpeedUpdateTime = Date.now();
let lastSpeedBytes = 0;
let speedHistory = [];
const MAX_SPEED_HISTORY = 3;
let downloadStartTime = null;
let taskSuccessNotified = false;

const SESSION_KEY_TASK_ID = 'narmusic_current_task_id';

let urlInput = null;
let filenameInput = null;
let sendBtn = null;
let localDownloadBtn = null;
let statusCard = null;
let statusLight = null;
let taskStatus = null;
let downloadedBytesEl = null;
let downloadSpeedEl = null;
let downloadTimeEl = null;
let downloadStatusEl = null;
let fileInfo = null;
let fileNameEl = null;
let fileSizeEl = null;

export function initDownload() {
    urlInput = document.getElementById('urlInput');
    filenameInput = document.getElementById('filenameInput');
    sendBtn = document.getElementById('sendBtn');
    localDownloadBtn = document.getElementById('localDownloadBtn');
    statusCard = document.getElementById('statusCard');
    statusLight = document.getElementById('statusLight');
    taskStatus = document.getElementById('taskStatus');
    downloadedBytesEl = document.getElementById('downloadedBytes');
    downloadSpeedEl = document.getElementById('downloadSpeed');
    downloadTimeEl = document.getElementById('downloadTime');
    downloadStatusEl = document.getElementById('downloadStatus');
    fileInfo = document.getElementById('fileInfo');
    fileNameEl = document.getElementById('fileName');
    fileSizeEl = document.getElementById('fileSize');

    if (sendBtn) sendBtn.addEventListener('click', handleSend);
    if (localDownloadBtn) localDownloadBtn.addEventListener('click', handleLocalDownload);
    if (urlInput) urlInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter' && e.ctrlKey) { e.preventDefault(); sendBtn.click(); }
    });
    if (filenameInput) filenameInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') { e.preventDefault(); sendBtn.click(); }
    });

    // 恢复上次未完成的下载任务
    const savedTaskId = sessionStorage.getItem(SESSION_KEY_TASK_ID);
    if (savedTaskId) {
        currentTaskId = savedTaskId;
        statusCard.style.display = 'block';
        updateStatusLight('downloading');
        downloadStartTime = Date.now();
        startPollingTaskStatus();
    }
}

async function handleSend() {
    const inputText = urlInput.value.trim();
    if (!inputText) { showToast('请输入内容', 'warning'); urlInput.focus(); return; }

    const url = extractUrlFromText(inputText);
    if (!url) { showToast('未找到有效的链接', 'warning'); urlInput.focus(); return; }

    const filename = filenameInput.value.trim();
    if (!filename) { showToast('请输入自定义文件名', 'warning'); filenameInput.focus(); return; }

    const { platform, delayMs, delayError } = getSettingsValues();
    if (delayError) { showToast('延迟参数必须是整数', 'warning'); return; }

    resetTaskStatus();
    statusCard.style.display = 'block';
    showToast('正在创建下载任务...', 'info');

    sendBtn.disabled = true;
    const originalBtnContent = sendBtn.innerHTML;
    sendBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">refresh</span> 发送中...';

    try {
        const requestData = { content: inputText, url, platform, offsetMs: delayMs, filename };

        const data = await api.createTask(requestData);

        if (data.task_id) {
            currentTaskId = data.task_id;
            sessionStorage.setItem(SESSION_KEY_TASK_ID, currentTaskId);
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
}

async function handleLocalDownload() {
    if (!currentTaskId) { showToast('没有可下载的文件', 'warning'); return; }

    localDownloadBtn.disabled = false;
    const originalContent = localDownloadBtn.innerHTML;

    const abortController = new AbortController();
    let progressEl = null;
    let cancelHandler = null;

    localDownloadBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">downloading</span> 准备中...';

    isLocalDownloading = true;
    cancelHandler = (e) => {
        e.stopImmediatePropagation();
        e.preventDefault();
        abortController.abort();
    };
    localDownloadBtn.addEventListener('click', cancelHandler, true);

    try {
        const response = await api.downloadFileByTask(currentTaskId, abortController.signal);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let filename = 'downloaded_music';
            if (contentDisposition) {
                filename = parseFilenameFromContentDisposition(contentDisposition, filename);
            }

            localDownloadBtn.innerHTML = '<span class="material-symbols-rounded">cancel</span> 下载中 <span class="dl-progress">0%</span>';
            progressEl = localDownloadBtn.querySelector('.dl-progress');

            const blob = await streamDownloadWithProgress(response, (received, total) => {
                if (abortController.signal.aborted) return;
                if (progressEl) {
                    if (total > 0) {
                        const percent = Math.round((received / total) * 100);
                        progressEl.textContent = percent + '%';
                    } else {
                        progressEl.textContent = formatBytes(received);
                    }
                }
            }, abortController.signal);

            if (blob) {
                triggerDownload(blob, filename);
                showToast('文件下载完成', 'success');
            } else {
                showToast('下载已取消', 'info');
            }
        } else {
            const errorData = await response.json();
            showToast('下载未完成: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        if (abortController.signal.aborted) {
            showToast('下载已取消', 'info');
        } else {
            showToast('下载错误: ' + error.message, 'warning');
        }
    } finally {
        isLocalDownloading = false;
        if (cancelHandler) localDownloadBtn.removeEventListener('click', cancelHandler, true);
        localDownloadBtn.disabled = false;
        localDownloadBtn.innerHTML = originalContent;
    }
}

function updateStatusLight(status) {
    statusLight.className = 'status-light ' + status;
    switch (status) {
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
    currentTaskId = null;
    sessionStorage.removeItem(SESSION_KEY_TASK_ID);
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
            const data = await api.getTaskStatus(currentTaskId);
            if (data.error === 'task_not_found') {
                showToast('任务不存在或已过期', 'warning');
                updateStatusLight('error');
                stopPolling();
            } else if (data.task_id) {
                updateTaskStatus(data);
            }
        } catch (error) {
            console.error('轮询错误:', error);
        }
    }, 1000);
}

function stopPolling() {
    if (pollingInterval) { clearInterval(pollingInterval); pollingInterval = null; }
    sessionStorage.removeItem(SESSION_KEY_TASK_ID);
}

function isDownloading() {
    return isLocalDownloading || !!pollingInterval;
}

export { stopPolling, isDownloading };

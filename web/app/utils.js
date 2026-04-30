export function formatBytes(bytes) {
    if (bytes === 0 || bytes === undefined) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    if (i === 0) return parseFloat(bytes.toFixed(2)) + ' ' + sizes[i];
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

export function formatSpeed(bytesPerSecond) {
    if (bytesPerSecond === 0) return '0 B/s';
    const k = 1024;
    const sizes = ['B/s', 'KB/s', 'MB/s'];
    let i = Math.floor(Math.log(bytesPerSecond) / Math.log(k));
    if (i >= sizes.length) i = sizes.length - 1;
    if (i === 0) return parseFloat(bytesPerSecond.toFixed(2)) + ' ' + sizes[i];
    return parseFloat((bytesPerSecond / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

export function formatTime(seconds) {
    if (seconds < 60) return seconds.toFixed(1) + '秒';
    if (seconds < 3600) return Math.floor(seconds / 60) + '分' + Math.floor(seconds % 60) + '秒';
    return Math.floor(seconds / 3600) + '小时' + Math.floor((seconds % 3600) / 60) + '分';
}

export function formatDuration(seconds) {
    if (!seconds || isNaN(seconds)) return '00:00';
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return String(mins).padStart(2, '0') + ':' + String(secs).padStart(2, '0');
}

export function formatDownloadTime(date) {
    const now = new Date();
    const diffMs = now - date;
    const diffDays = Math.floor(diffMs / (1000 * 60 * 60 * 24));
    if (diffDays === 0) return '今天';
    if (diffDays === 1) return '昨天';
    if (diffDays < 7) return diffDays + '天前';
    if (diffDays < 30) return Math.floor(diffDays / 7) + '周前';
    return date.toLocaleDateString('zh-CN', { year: 'numeric', month: 'short', day: 'numeric' });
}

export function formatDateTime(date) {
    const year = date.getFullYear();
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hours = String(date.getHours()).padStart(2, '0');
    const minutes = String(date.getMinutes()).padStart(2, '0');
    return year + '-' + month + '-' + day + ' ' + hours + ':' + minutes;
}

export function extractUrlFromText(text) {
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

export function escapeHtml(str) {
    if (!str) return '';
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

let toastTimeout = null;

const TOAST_DURATIONS = { info: 800, success: 1000, warning: 1500, error: 2000 };

export function showToast(message, type = 'info', duration) {
    if (duration === undefined) duration = TOAST_DURATIONS[type] || 2000;
    const toast = document.getElementById('toast');
    if (!toast) return;
    if (toast.classList.contains('show')) {
        toast.classList.remove('show');
        if (toastTimeout) clearTimeout(toastTimeout);
        toastTimeout = setTimeout(() => {
            _showNewToast(message, type, duration);
        }, 300);
    } else {
        _showNewToast(message, type, duration);
    }
}

function _showNewToast(message, type, duration) {
    const toast = document.getElementById('toast');
    if (!toast) return;
    toast.textContent = message;
    toast.className = 'toast ' + type;
    toast.onclick = () => {
        toast.classList.remove('show');
        toast.onclick = null;
        if (toastTimeout) clearTimeout(toastTimeout);
    };
    setTimeout(() => toast.classList.add('show'), 10);
    toastTimeout = setTimeout(() => {
        toast.classList.remove('show');
        toast.onclick = null;
    }, duration);
}

export function triggerDownload(blob, filename) {
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    window.URL.revokeObjectURL(url);
    document.body.removeChild(a);
}

export async function streamDownloadWithProgress(response, onProgress, signal) {
    const contentLength = parseInt(response.headers.get('Content-Length') || '0', 10);
    const reader = response.body.getReader();
    const chunks = [];
    let receivedLength = 0;
    let lastNotifyTime = 0;

    if (signal) {
        signal.addEventListener('abort', () => {
            reader.cancel();
        }, { once: true });
    }

    try {
        while (true) {
            const { done, value } = await reader.read();
            if (done) break;
            if (signal && signal.aborted) break;
            chunks.push(value);
            receivedLength += value.length;
            if (onProgress) {
                const now = Date.now();
                if (now - lastNotifyTime >= 200) {
                    lastNotifyTime = now;
                    onProgress(receivedLength, contentLength);
                }
            }
        }
    } catch (e) {
        if (signal && signal.aborted) return null;
        throw e;
    }

    if (signal && signal.aborted) return null;

    if (onProgress && receivedLength > 0) {
        onProgress(receivedLength, contentLength);
    }

    const blob = new Blob(chunks);
    return blob;
}

export function parseFilenameFromContentDisposition(contentDisposition, fallback) {
    if (!contentDisposition) return fallback;
    const utf8Match = /filename\*=UTF-8''(.+)/i.exec(contentDisposition);
    if (utf8Match && utf8Match[1]) {
        try { return decodeURIComponent(utf8Match[1]); } catch { return utf8Match[1]; }
    }
    const matches = /filename="?([^"]+)"?/i.exec(contentDisposition);
    if (matches && matches[1]) {
        try { return decodeURIComponent(matches[1]); } catch { return matches[1]; }
    }
    return fallback;
}

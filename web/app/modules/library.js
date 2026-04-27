import { api } from '../api.js';
import { store } from '../state.js';
import { formatBytes, formatDateTime, escapeHtml, showToast, parseFilenameFromContentDisposition, triggerDownload } from '../utils.js';
import { playMusicFromLibrary } from './player.js';

let musicLibrary = [];
let isLibraryLoading = false;
let isBatchMode = false;
let selectedMusicIds = new Set();

export function initLibrary() {
    const refreshLibraryBtn = document.getElementById('refreshLibraryBtn');
    if (refreshLibraryBtn) {
        refreshLibraryBtn.addEventListener('click', loadMusicLibrary);
    }

    const batchSelectBtn = document.getElementById('batchSelectBtn');
    if (batchSelectBtn) {
        batchSelectBtn.addEventListener('click', toggleBatchMode);
    }

    const selectAllCheckbox = document.getElementById('selectAllCheckbox');
    if (selectAllCheckbox) {
        selectAllCheckbox.addEventListener('change', toggleSelectAll);
    }

    const batchDownloadBtn = document.getElementById('batchDownloadBtn');
    if (batchDownloadBtn) {
        batchDownloadBtn.addEventListener('click', batchDownload);
    }

    const batchDeleteBtn = document.getElementById('batchDeleteBtn');
    if (batchDeleteBtn) {
        batchDeleteBtn.addEventListener('click', batchDelete);
    }

    const cancelBatchBtn = document.getElementById('cancelBatchBtn');
    if (cancelBatchBtn) {
        cancelBatchBtn.addEventListener('click', toggleBatchMode);
    }

    document.addEventListener('tab:library', () => loadMusicLibrary());

    const libraryEl = document.getElementById('musicLibrary');
    if (libraryEl) {
        libraryEl.addEventListener('click', handleLibraryClick);
    }
}

function handleLibraryClick(e) {
    const target = e.target.closest('[data-action]');
    if (!target) return;
    const action = target.dataset.action;
    const id = parseInt(target.dataset.id);
    const filename = target.dataset.filename;

    if (action === 'play') {
        playMusicFromLibrary(filename, musicLibrary);
    } else if (action === 'download') {
        downloadMusicFile(filename);
    } else if (action === 'delete') {
        deleteMusicFromLibrary(id, filename);
    } else if (action === 'select') {
        const checkbox = target;
        toggleMusicSelect(id, checkbox.checked);
    }
}

export async function loadMusicLibrary() {
    if (isLibraryLoading) return;

    isLibraryLoading = true;
    const musicLibraryEl = document.getElementById('musicLibrary');
    const refreshBtn = document.getElementById('refreshLibraryBtn');
    const originalContent = refreshBtn ? refreshBtn.innerHTML : '';

    if (refreshBtn) {
        refreshBtn.disabled = true;
        refreshBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">refresh</span> 加载中...';
    }
    musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block; animation: spin 1s linear infinite;">refresh</span><div style="font-size: 16px; font-weight: 500;">加载音乐库中...</div></div>';

    try {
        const response = await api.libraryList();
        if (response.ok) {
            musicLibrary = await response.json();
            store.set('musicLibrary', musicLibrary);
            renderMusicLibrary();
            showToast('音乐库加载成功', 'success');
        } else {
            const errorData = await response.json();
            showToast('加载失败: ' + (errorData.error || '未知错误'), 'warning');
            musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">error</span><div style="font-size: 16px; font-weight: 500;">加载失败</div><div style="font-size: 14px; margin-top: 8px;">' + escapeHtml(errorData.error || '请重试') + '</div></div>';
        }
    } catch (error) {
        showToast('加载失败: ' + error.message, 'warning');
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">wifi_off</span><div style="font-size: 16px; font-weight: 500;">网络连接失败</div><div style="font-size: 14px; margin-top: 8px;">请检查服务器是否运行</div></div>';
    } finally {
        isLibraryLoading = false;
        if (refreshBtn) {
            refreshBtn.disabled = false;
            refreshBtn.innerHTML = originalContent;
        }
    }
}

function renderMusicLibrary() {
    const musicLibraryEl = document.getElementById('musicLibrary');
    const musicCountEl = document.getElementById('musicCount');

    if (!musicLibrary || musicLibrary.length === 0) {
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">library_music</span><div style="font-size: 16px; font-weight: 500;">音乐库为空</div><div style="font-size: 14px; margin-top: 8px;">前往下载页面添加音乐</div></div>';
        musicCountEl.textContent = '0';
        if (isBatchMode) toggleBatchMode();
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

        const musicId = music.id;
        const isSelected = selectedMusicIds.has(musicId);

        html += '<div class="music-item' + (isSelected ? ' batch-selected' : '') + '" data-index="' + index + '" data-id="' + musicId + '">'
            + '<div class="music-index">'
            + '<input type="checkbox" class="music-checkbox" data-id="' + musicId + '" data-action="select" ' + (isSelected ? 'checked' : '') + '>'
            + '<span class="music-index-num">' + indexNum + '</span>'
            + '<button class="music-index-play" title="播放" data-action="play" data-filename="' + escapeAttr(systemFilename) + '">'
            + '<span class="material-symbols-rounded">play_arrow</span>'
            + '</button>'
            + '</div>'
            + '<div class="music-name">'
            + '<div class="music-title" title="' + escapeAttr(title) + '">' + escapeHtml(title) + '</div>'
            + '<div class="music-subtitle">' + delayMs + 'ms · ' + sizeText + '</div>'
            + '</div>'
            + '<div class="music-delay">' + delayMs + 'ms</div>'
            + '<div class="music-size">' + sizeText + '</div>'
            + '<div class="music-time">' + timeText + '</div>'
            + '<div class="music-actions-cell">'
            + '<button class="music-action-btn" title="下载" data-action="download" data-filename="' + escapeAttr(systemFilename) + '">'
            + '<span class="material-symbols-rounded">download</span>'
            + '</button>'
            + '<button class="music-action-btn" title="删除" data-action="delete" data-id="' + musicId + '" data-filename="' + escapeAttr(systemFilename) + '">'
            + '<span class="material-symbols-rounded">delete</span>'
            + '</button>'
            + '</div>'
            + '</div>';
    });

    musicLibraryEl.innerHTML = html;

    if (isBatchMode) {
        musicLibraryEl.classList.add('batch-mode');
    }
}

function escapeAttr(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

async function downloadMusicFile(systemFilename) {
    if (!systemFilename) { showToast('文件名无效', 'warning'); return; }
    showToast('准备下载文件...', 'info');

    try {
        const response = await api.downloadFileByName(systemFilename);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let downloadFilename = systemFilename;
            const musicItem = musicLibrary.find(item => item.system_filename === systemFilename);
            if (musicItem && musicItem.custom_filename) {
                downloadFilename = musicItem.custom_filename;
            }
            if (contentDisposition) {
                downloadFilename = parseFilenameFromContentDisposition(contentDisposition, downloadFilename);
            }
            const blob = await response.blob();
            triggerDownload(blob, downloadFilename);
            showToast('文件开始下载到您的设备', 'success');
        } else {
            const errorData = await response.json();
            showToast('下载未完成: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('下载错误: ' + error.message, 'warning');
    }
}

async function deleteMusicFromLibrary(musicId, systemFilename) {
    if (!musicId) { showToast('文件ID无效', 'warning'); return; }
    const title = systemFilename.replace(/\.[^/.]+$/, '') || '该文件';
    if (!confirm('确定要删除「' + title + '」吗？此操作不可恢复。')) return;

    try {
        const response = await api.libraryDelete(musicId);
        if (response.ok) {
            showToast('已删除', 'success');
            loadMusicLibrary();
        } else {
            const errorData = await response.json();
            showToast('删除失败: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('删除错误: ' + error.message, 'warning');
    }
}

function toggleBatchMode() {
    isBatchMode = !isBatchMode;
    selectedMusicIds.clear();

    const musicLibraryEl = document.getElementById('musicLibrary');
    const batchToolbar = document.getElementById('batchToolbar');
    const batchSelectBtn = document.getElementById('batchSelectBtn');

    if (isBatchMode) {
        musicLibraryEl.classList.add('batch-mode');
        batchToolbar.style.display = 'flex';
        batchSelectBtn.innerHTML = '<span class="material-symbols-rounded">checklist</span> 取消批量';
        batchSelectBtn.classList.add('active');
    } else {
        musicLibraryEl.classList.remove('batch-mode');
        batchToolbar.style.display = 'none';
        batchSelectBtn.innerHTML = '<span class="material-symbols-rounded">checklist</span> 批量选择';
        batchSelectBtn.classList.remove('active');
    }

    updateBatchUI();
    renderMusicLibrary();
}

function toggleMusicSelect(musicId, checked) {
    if (checked) {
        selectedMusicIds.add(musicId);
    } else {
        selectedMusicIds.delete(musicId);
    }
    const item = document.querySelector('.music-item[data-id="' + musicId + '"]');
    if (item) {
        if (checked) item.classList.add('batch-selected');
        else item.classList.remove('batch-selected');
    }
    updateBatchUI();
}

function toggleSelectAll() {
    const selectAllCheckbox = document.getElementById('selectAllCheckbox');
    const allIds = musicLibrary.map(m => m.id);

    if (selectAllCheckbox.checked) {
        allIds.forEach(id => selectedMusicIds.add(id));
    } else {
        selectedMusicIds.clear();
    }

    document.querySelectorAll('.music-checkbox').forEach(cb => {
        const id = parseInt(cb.dataset.id);
        cb.checked = selectAllCheckbox.checked;
        const item = cb.closest('.music-item');
        if (item) {
            if (selectAllCheckbox.checked) item.classList.add('batch-selected');
            else item.classList.remove('batch-selected');
        }
    });

    updateBatchUI();
}

function updateBatchUI() {
    const selectedCountEl = document.getElementById('selectedCount');
    const batchDownloadBtn = document.getElementById('batchDownloadBtn');
    const batchDeleteBtn = document.getElementById('batchDeleteBtn');
    const selectAllCheckbox = document.getElementById('selectAllCheckbox');

    const count = selectedMusicIds.size;
    selectedCountEl.textContent = '已选 ' + count + ' 项';
    batchDownloadBtn.disabled = count === 0;
    batchDeleteBtn.disabled = count === 0;

    const allIds = musicLibrary.map(m => m.id);
    if (allIds.length > 0 && count === allIds.length) {
        selectAllCheckbox.checked = true;
    } else {
        selectAllCheckbox.checked = false;
    }
}

async function batchDownload() {
    const ids = Array.from(selectedMusicIds);
    if (ids.length === 0) { showToast('请先选择要下载的文件', 'warning'); return; }

    showToast('正在准备 ' + ids.length + ' 个文件...', 'info');

    const batchDownloadBtn = document.getElementById('batchDownloadBtn');
    batchDownloadBtn.disabled = true;
    const originalContent = batchDownloadBtn.innerHTML;
    batchDownloadBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">hourglass_top</span> 准备中...';

    try {
        const response = await api.libraryBatchDownload(ids);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let filename = 'NarMusic_download.zip';
            if (contentDisposition) {
                filename = parseFilenameFromContentDisposition(contentDisposition, filename);
            }
            const blob = await response.blob();
            triggerDownload(blob, filename);
            showToast((ids.length === 1 ? '文件' : ids.length + ' 个文件打包') + '下载已开始', 'success');
        } else {
            const errorData = await response.json();
            showToast('下载失败: ' + (errorData.message || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('下载错误: ' + error.message, 'warning');
    } finally {
        batchDownloadBtn.disabled = false;
        batchDownloadBtn.innerHTML = originalContent;
    }
}

async function batchDelete() {
    const ids = Array.from(selectedMusicIds);
    if (ids.length === 0) { showToast('请先选择要删除的文件', 'warning'); return; }
    if (!confirm('确定要删除选中的 ' + ids.length + ' 个文件吗？此操作不可恢复。')) return;

    showToast('正在删除 ' + ids.length + ' 个文件...', 'info');

    const batchDeleteBtn = document.getElementById('batchDeleteBtn');
    batchDeleteBtn.disabled = true;
    const originalContent = batchDeleteBtn.innerHTML;
    batchDeleteBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">hourglass_top</span> 删除中...';

    try {
        const response = await api.libraryBatchDelete(ids);
        if (response.ok) {
            const data = await response.json();
            showToast('已删除 ' + (data.count || ids.length) + ' 个文件', 'success');
            selectedMusicIds.clear();
            loadMusicLibrary();
        } else {
            const errorData = await response.json();
            showToast('删除失败: ' + (errorData.message || '未知错误'), 'warning');
        }
    } catch (error) {
        showToast('删除错误: ' + error.message, 'warning');
    } finally {
        batchDeleteBtn.disabled = false;
        batchDeleteBtn.innerHTML = originalContent;
    }
}

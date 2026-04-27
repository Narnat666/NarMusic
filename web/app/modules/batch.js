import { api } from '../api.js';
import { escapeHtml, showToast } from '../utils.js';
import { getSettingsValues } from './settings.js';

let batchResults = [];
let batchSelectedIndexes = new Set();
let batchCurrentInputMode = 'paste';
let batchIsParsing = false;
let batchImportedText = '';

export function initBatch() {
    const pasteModeBtn = document.getElementById('pasteModeBtn');
    const importModeBtn = document.getElementById('importModeBtn');
    if (pasteModeBtn) pasteModeBtn.addEventListener('click', () => switchBatchInputMode('paste'));
    if (importModeBtn) importModeBtn.addEventListener('click', () => switchBatchInputMode('import'));

    const batchParseBtn = document.getElementById('batchParseBtn');
    if (batchParseBtn) batchParseBtn.addEventListener('click', parseBatchPlaylist);

    const batchClearBtn = document.getElementById('batchClearBtn');
    if (batchClearBtn) batchClearBtn.addEventListener('click', clearBatchInput);

    const batchSelectAllCheckbox = document.getElementById('batchSelectAllCheckbox');
    if (batchSelectAllCheckbox) batchSelectAllCheckbox.addEventListener('change', toggleBatchSelectAll);

    const batchDownloadServerBtn = document.getElementById('batchDownloadServerBtn');
    if (batchDownloadServerBtn) batchDownloadServerBtn.addEventListener('click', batchDownloadToServer);

    const batchDeleteSelectedBtn = document.getElementById('batchDeleteSelectedBtn');
    if (batchDeleteSelectedBtn) batchDeleteSelectedBtn.addEventListener('click', batchDeleteSelected);

    initBatchFileUpload();

    const resultsList = document.getElementById('batchResultsList');
    if (resultsList) {
        resultsList.addEventListener('click', handleBatchResultsClick);
        resultsList.addEventListener('change', handleBatchResultsChange);
    }

    const clearFileBtn = document.querySelector('.batch-file-remove');
    if (clearFileBtn) {
        clearFileBtn.addEventListener('click', (e) => {
            e.preventDefault();
            clearBatchFile();
        });
    }
}

function handleBatchResultsClick(e) {
    const target = e.target.closest('[data-action]');
    if (!target) return;
    const action = target.dataset.action;
    const idx = parseInt(target.dataset.idx);

    if (action === 'delete-batch-item') {
        deleteBatchItem(idx);
    }
}

function handleBatchResultsChange(e) {
    const target = e.target.closest('[data-action]');
    if (!target) return;
    const action = target.dataset.action;
    const idx = parseInt(target.dataset.idx);

    if (action === 'select-batch-item') {
        toggleBatchItemSelect(idx, target.checked);
    }
}

function switchBatchInputMode(mode) {
    batchCurrentInputMode = mode;
    const pasteModeBtn = document.getElementById('pasteModeBtn');
    const importModeBtn = document.getElementById('importModeBtn');
    const pasteModeArea = document.getElementById('pasteModeArea');
    const importModeArea = document.getElementById('importModeArea');

    if (mode === 'paste') {
        pasteModeBtn.classList.add('active');
        importModeBtn.classList.remove('active');
        pasteModeArea.style.display = 'block';
        importModeArea.style.display = 'none';
    } else {
        importModeBtn.classList.add('active');
        pasteModeBtn.classList.remove('active');
        pasteModeArea.style.display = 'none';
        importModeArea.style.display = 'block';
    }
}

function initBatchFileUpload() {
    const uploadArea = document.getElementById('batchFileUpload');
    const fileInput = document.getElementById('batchFileInput');
    if (!uploadArea || !fileInput) return;

    uploadArea.addEventListener('click', () => fileInput.click());

    uploadArea.addEventListener('dragover', (e) => {
        e.preventDefault();
        uploadArea.classList.add('drag-over');
    });

    uploadArea.addEventListener('dragleave', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('drag-over');
    });

    uploadArea.addEventListener('drop', (e) => {
        e.preventDefault();
        uploadArea.classList.remove('drag-over');
        const files = e.dataTransfer.files;
        if (files.length > 0 && files[0].name.endsWith('.txt')) {
            handleBatchFile(files[0]);
        } else {
            showToast('请上传 .txt 文件', 'warning');
        }
    });

    fileInput.addEventListener('change', (e) => {
        if (e.target.files.length > 0) {
            handleBatchFile(e.target.files[0]);
        }
    });
}

function handleBatchFile(file) {
    const reader = new FileReader();
    reader.onload = (e) => {
        batchImportedText = e.target.result;
        const fileNameEl = document.getElementById('batchFileName');
        const fileNameText = document.getElementById('batchFileNameText');
        fileNameText.textContent = file.name;
        fileNameEl.style.display = 'flex';
        showToast('文件已导入: ' + file.name, 'success');
    };
    reader.onerror = () => showToast('文件读取失败', 'warning');
    reader.readAsText(file);
}

function clearBatchFile() {
    batchImportedText = '';
    const fileNameEl = document.getElementById('batchFileName');
    const fileInput = document.getElementById('batchFileInput');
    fileNameEl.style.display = 'none';
    fileInput.value = '';
}

function clearBatchInput() {
    const pasteInput = document.getElementById('batchPasteInput');
    if (pasteInput) pasteInput.value = '';
    clearBatchFile();
    batchResults = [];
    batchSelectedIndexes.clear();
    document.getElementById('batchResultsArea').style.display = 'none';
    document.getElementById('batchParseProgress').style.display = 'none';
    showToast('已清空', 'info');
}

function parsePlaylistText(text) {
    const lines = text.split('\n').map(l => l.trim()).filter(l => l.length > 0);
    const songs = [];

    for (const line of lines) {
        if (line.includes('|')) {
            const parts = line.split('|');
            if (parts.length >= 4 && parts[0].match(/^https?:\/\//)) {
                const url = parts[0].trim();
                const platform = parts[1].trim();
                const delayStr = parts[2].trim();
                const filename = parts.slice(3).join('|').trim();
                const delayMs = parseInt(delayStr) || 0;
                let title = '';
                let artist = '';
                if (filename.includes(' - ')) {
                    const fparts = filename.split(' - ');
                    title = fparts[0].trim();
                    artist = fparts.slice(1).join(' - ').trim();
                } else {
                    title = filename;
                }
                const keyword = artist ? title + ' ' + artist : title;
                songs.push({ title, artist, keyword, original: line, narmeta: true, url, platform, delayMs, filename });
                continue;
            }
        }

        let title = '';
        let artist = '';
        if (line.includes(' - ')) {
            const parts = line.split(' - ');
            title = parts[0].trim();
            artist = parts.slice(1).join(' - ').trim();
        } else if (line.includes('-')) {
            const parts = line.split('-');
            title = parts[0].trim();
            artist = parts.slice(1).join('-').trim();
        } else {
            title = line;
        }

        if (title) {
            const keyword = artist ? title + ' ' + artist : title;
            songs.push({ title, artist, keyword, original: line, narmeta: false });
        }
    }

    return songs;
}

async function parseBatchPlaylist() {
    if (batchIsParsing) return;

    let text = '';
    if (batchCurrentInputMode === 'paste') {
        text = document.getElementById('batchPasteInput').value.trim();
    } else {
        text = batchImportedText;
    }

    if (!text) { showToast('请先输入或导入歌单内容', 'warning'); return; }

    const songs = parsePlaylistText(text);
    if (songs.length === 0) { showToast('未解析到有效歌曲', 'warning'); return; }

    batchIsParsing = true;
    batchResults = [];
    batchSelectedIndexes.clear();

    const parseBtn = document.getElementById('batchParseBtn');
    parseBtn.disabled = true;
    const originalContent = parseBtn.innerHTML;
    parseBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">progress_activity</span> 解析中...';

    const progressEl = document.getElementById('batchParseProgress');
    const progressText = document.getElementById('batchProgressText');
    const progressFill = document.getElementById('batchProgressFill');
    const progressStats = document.getElementById('batchProgressStats');
    progressEl.style.display = 'block';
    document.getElementById('batchResultsArea').style.display = 'none';

    const narmetaSongs = songs.filter(s => s.narmeta);
    const searchSongs = songs.filter(s => !s.narmeta);

    let foundCount = 0;
    let notFoundCount = 0;

    for (const song of narmetaSongs) {
        batchResults.push({
            index: batchResults.length, title: song.title, artist: song.artist,
            original: song.original, keyword: song.keyword, found: true,
            link: song.url, searchTitle: song.title, status: 'found',
            narmeta: true, narmetaPlatform: song.platform,
            narmetaDelayMs: song.delayMs, narmetaFilename: song.filename
        });
        foundCount++;
    }

    if (searchSongs.length > 0) {
        progressText.textContent = '正在批量搜索歌单...';
        progressFill.style.width = '30%';
        progressStats.textContent = '0 / ' + searchSongs.length;

        try {
            const keywords = searchSongs.map(s => s.keyword);
            const response = await api.batchSearch(keywords);
            const data = await response.json();

            if (response.ok && data.results) {
                for (let i = 0; i < data.results.length; i++) {
                    const result = data.results[i];
                    const song = searchSongs[i];
                    if (result.found) {
                        batchResults.push({
                            index: batchResults.length, title: song.title, artist: song.artist,
                            original: song.original, keyword: song.keyword, found: true,
                            link: result.link, searchTitle: result.title || song.title,
                            status: 'found', narmeta: false
                        });
                        foundCount++;
                    } else {
                        batchResults.push({
                            index: batchResults.length, title: song.title, artist: song.artist,
                            original: song.original, keyword: song.keyword, found: false,
                            link: '', searchTitle: '', status: 'notfound', narmeta: false
                        });
                        notFoundCount++;
                    }
                }
            } else {
                showToast('批量搜索失败: ' + (data.message || data.error || '未知错误'), 'warning');
                for (const song of searchSongs) {
                    batchResults.push({
                        index: batchResults.length, title: song.title, artist: song.artist,
                        original: song.original, keyword: song.keyword, found: false,
                        link: '', searchTitle: '', status: 'notfound', narmeta: false
                    });
                    notFoundCount++;
                }
            }
        } catch (error) {
            showToast('批量搜索失败: ' + error.message, 'warning');
            for (const song of searchSongs) {
                batchResults.push({
                    index: batchResults.length, title: song.title, artist: song.artist,
                    original: song.original, keyword: song.keyword, found: false,
                    link: '', searchTitle: '', status: 'notfound', narmeta: false
                });
                notFoundCount++;
            }
        }
    }

    progressFill.style.width = '100%';
    progressStats.textContent = songs.length + ' / ' + songs.length;

    document.getElementById('batchTotalCount').textContent = batchResults.length;
    document.getElementById('batchFoundCount').textContent = foundCount;
    document.getElementById('batchNotFoundCount').textContent = notFoundCount;
    document.getElementById('batchResultsArea').style.display = 'block';

    renderBatchResults();
    const narmetaCount = narmetaSongs.length;
    let msg = '解析完成: 找到 ' + foundCount + ' 首, 未找到 ' + notFoundCount + ' 首';
    if (narmetaCount > 0) msg += ' (含 ' + narmetaCount + ' 首精确匹配)';
    showToast(msg, foundCount > 0 ? 'success' : 'warning');

    progressEl.style.display = 'none';
    parseBtn.disabled = false;
    parseBtn.innerHTML = originalContent;
    batchIsParsing = false;
}

function renderBatchResults() {
    const listEl = document.getElementById('batchResultsList');

    if (batchResults.length === 0) {
        listEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">search_off</span><div style="font-size: 16px; font-weight: 500;">暂无搜索结果</div></div>';
        return;
    }

    let html = '';
    batchResults.forEach((item, idx) => {
        const indexNum = String(idx + 1).padStart(2, '0');
        const isSelected = batchSelectedIndexes.has(idx);
        const isFound = item.found;
        const notFoundClass = !isFound ? ' batch-item-notfound' : '';
        const selectedClass = isSelected ? ' batch-item-selected' : '';

        let statusHtml = '';
        if (item.status === 'found') {
            if (item.narmeta) {
                statusHtml = '<span class="batch-item-status found"><span class="material-symbols-rounded" style="font-size: 14px;">verified</span>精确匹配</span>';
            } else {
                statusHtml = '<span class="batch-item-status found"><span class="material-symbols-rounded" style="font-size: 14px;">check_circle</span>已找到</span>';
            }
        } else if (item.status === 'downloading') {
            statusHtml = '<span class="batch-item-status downloading"><span class="material-symbols-rounded" style="font-size: 14px; animation: spin 1s linear infinite;">progress_activity</span>下载中</span>';
        } else if (item.status === 'downloaded') {
            statusHtml = '<span class="batch-item-status downloaded"><span class="material-symbols-rounded" style="font-size: 14px;">cloud_done</span>已下载</span>';
        } else if (item.status === 'failed') {
            statusHtml = '<span class="batch-item-status notfound"><span class="material-symbols-rounded" style="font-size: 14px;">error</span>失败</span>';
        } else {
            statusHtml = '<span class="batch-item-status notfound"><span class="material-symbols-rounded" style="font-size: 14px;">cancel</span>未找到</span>';
        }

        const artistInfo = item.narmeta
            ? (item.narmetaPlatform + ' · ' + (item.narmetaDelayMs >= 0 ? '+' : '') + item.narmetaDelayMs + 'ms')
            : (item.artist || '未知歌手');

        html += '<div class="batch-result-item' + notFoundClass + selectedClass + '" data-idx="' + idx + '">'
            + '<div class="batch-item-index">' + indexNum + '</div>'
            + '<div class="batch-item-checkbox">'
            + '<input type="checkbox" data-idx="' + idx + '" data-action="select-batch-item" ' + (isSelected ? 'checked' : '') + ' ' + (!isFound ? 'disabled' : '') + '>'
            + '</div>'
            + '<div class="batch-item-info">'
            + '<div class="batch-item-title" title="' + escapeAttr(item.searchTitle || item.title) + '">' + escapeHtml(item.searchTitle || item.title) + '</div>'
            + '<div class="batch-item-artist">' + escapeHtml(artistInfo) + '</div>'
            + '</div>'
            + statusHtml
            + '<div class="batch-item-actions">'
            + '<button class="batch-item-delete" title="删除" data-action="delete-batch-item" data-idx="' + idx + '">'
            + '<span class="material-symbols-rounded">close</span>'
            + '</button>'
            + '</div>'
            + '</div>';
    });

    listEl.innerHTML = html;
    updateBatchEditUI();
}

function escapeAttr(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/'/g, '&#39;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function toggleBatchItemSelect(idx, checked) {
    if (checked) batchSelectedIndexes.add(idx);
    else batchSelectedIndexes.delete(idx);

    const item = document.querySelector('.batch-result-item[data-idx="' + idx + '"]');
    if (item) {
        if (checked) item.classList.add('batch-item-selected');
        else item.classList.remove('batch-item-selected');
    }
    updateBatchEditUI();
}

function toggleBatchSelectAll() {
    const checkbox = document.getElementById('batchSelectAllCheckbox');
    const allFoundIndexes = batchResults.map((item, idx) => item.found ? idx : -1).filter(idx => idx >= 0);

    if (checkbox.checked) {
        allFoundIndexes.forEach(idx => batchSelectedIndexes.add(idx));
        document.querySelectorAll('.batch-result-item .batch-item-checkbox input:not(:disabled)').forEach(cb => {
            cb.checked = true;
            const itemEl = cb.closest('.batch-result-item');
            if (itemEl) itemEl.classList.add('batch-item-selected');
        });
    } else {
        batchSelectedIndexes.clear();
        document.querySelectorAll('.batch-result-item .batch-item-checkbox input').forEach(cb => {
            cb.checked = false;
            const itemEl = cb.closest('.batch-result-item');
            if (itemEl) itemEl.classList.remove('batch-item-selected');
        });
    }
    updateBatchEditUI();
}

function updateBatchEditUI() {
    const count = batchSelectedIndexes.size;
    const foundCount = batchResults.filter(item => item.found).length;
    document.getElementById('batchSelectedCount').textContent = '已选 ' + count + ' 项';
    document.getElementById('batchDownloadServerBtn').disabled = count === 0;
    document.getElementById('batchDeleteSelectedBtn').disabled = count === 0;

    const selectAllCheckbox = document.getElementById('batchSelectAllCheckbox');
    if (foundCount > 0 && count === foundCount) {
        selectAllCheckbox.checked = true;
    } else {
        selectAllCheckbox.checked = false;
    }
}

function deleteBatchItem(idx) {
    batchResults.splice(idx, 1);
    const newSelected = new Set();
    batchSelectedIndexes.forEach(i => {
        if (i < idx) newSelected.add(i);
        else if (i > idx) newSelected.add(i - 1);
    });
    batchSelectedIndexes = newSelected;

    const foundCount = batchResults.filter(item => item.found).length;
    const notFoundCount = batchResults.filter(item => !item.found).length;
    document.getElementById('batchTotalCount').textContent = batchResults.length;
    document.getElementById('batchFoundCount').textContent = foundCount;
    document.getElementById('batchNotFoundCount').textContent = notFoundCount;

    if (batchResults.length === 0) {
        document.getElementById('batchResultsArea').style.display = 'none';
        showToast('歌单已清空', 'info');
    } else {
        renderBatchResults();
    }
}

function batchDeleteSelected() {
    const indexes = Array.from(batchSelectedIndexes).sort((a, b) => b - a);
    if (indexes.length === 0) return;
    if (!confirm('确定要删除选中的 ' + indexes.length + ' 首歌曲吗？')) return;

    for (const idx of indexes) {
        batchResults.splice(idx, 1);
    }
    batchSelectedIndexes.clear();

    const foundCount = batchResults.filter(item => item.found).length;
    const notFoundCount = batchResults.filter(item => !item.found).length;
    document.getElementById('batchTotalCount').textContent = batchResults.length;
    document.getElementById('batchFoundCount').textContent = foundCount;
    document.getElementById('batchNotFoundCount').textContent = notFoundCount;

    if (batchResults.length === 0) {
        document.getElementById('batchResultsArea').style.display = 'none';
        showToast('歌单已清空', 'info');
    } else {
        renderBatchResults();
        showToast('已删除 ' + indexes.length + ' 首歌曲', 'success');
    }
}

async function batchDownloadToServer() {
    const indexes = Array.from(batchSelectedIndexes).filter(idx => batchResults[idx] && batchResults[idx].found);
    if (indexes.length === 0) { showToast('请先选择要下载的歌曲', 'warning'); return; }

    const downloadBtn = document.getElementById('batchDownloadServerBtn');
    downloadBtn.disabled = true;
    const originalContent = downloadBtn.innerHTML;
    downloadBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">progress_activity</span> 下载中...';

    showToast('正在提交 ' + indexes.length + ' 首歌曲下载到服务器...', 'info');

    const { platform, delayMs } = getSettingsValues();

    const tasks = indexes.map(idx => {
        const item = batchResults[idx];
        if (item.narmeta) {
            return {
                url: item.link, content: item.link,
                filename: item.narmetaFilename || (item.title + (item.artist ? ' - ' + item.artist : '')),
                keyword: item.keyword, title: item.searchTitle || item.title,
                platform: item.narmetaPlatform, offsetMs: item.narmetaDelayMs
            };
        }
        return {
            url: item.link, content: item.link,
            filename: item.title + (item.artist ? ' - ' + item.artist : ''),
            keyword: item.keyword, title: item.searchTitle || item.title
        };
    });

    indexes.forEach(idx => { batchResults[idx].status = 'downloading'; });
    renderBatchResults();

    try {
        const response = await api.batchDownload(tasks, platform, delayMs);
        const data = await response.json();

        if (response.ok) {
            const results = data.results || [];
            let submitCount = 0;
            let failCount = 0;
            const pollTasks = [];

            for (let i = 0; i < indexes.length; i++) {
                const idx = indexes[i];
                if (results[i]) {
                    if (results[i].success) {
                        batchResults[idx].taskId = results[i].task_id;
                        batchResults[idx].status = 'downloading';
                        pollTasks.push({ idx, taskId: results[i].task_id });
                        submitCount++;
                    } else {
                        batchResults[idx].status = 'failed';
                        batchResults[idx].errorMsg = results[i].error || '提交失败';
                        failCount++;
                    }
                } else {
                    batchResults[idx].status = 'failed';
                    batchResults[idx].errorMsg = '提交失败';
                    failCount++;
                }
            }

            renderBatchResults();

            if (submitCount > 0) {
                showToast('已提交 ' + submitCount + ' 首歌曲下载' + (failCount > 0 ? '，' + failCount + ' 首提交失败' : ''), 'info');
                pollBatchDownloadStatus(pollTasks);
            } else {
                showToast('所有歌曲下载提交失败', 'warning');
            }
        } else {
            indexes.forEach(idx => {
                batchResults[idx].status = 'failed';
                batchResults[idx].errorMsg = data.error || '请求失败';
            });
            renderBatchResults();
            showToast('批量下载失败: ' + (data.message || data.error || '未知错误'), 'warning');
        }
    } catch (error) {
        indexes.forEach(idx => {
            batchResults[idx].status = 'failed';
            batchResults[idx].errorMsg = error.message;
        });
        renderBatchResults();
        showToast('批量下载错误: ' + error.message, 'warning');
    }

    downloadBtn.disabled = false;
    downloadBtn.innerHTML = originalContent;
}

function pollBatchDownloadStatus(pollTasks) {
    const pending = new Set(pollTasks.map(t => t.taskId));
    const taskMap = {};
    pollTasks.forEach(t => { taskMap[t.taskId] = t.idx; });

    let needsRender = false;
    let renderTimeout = null;

    const scheduleRender = () => {
        if (renderTimeout) clearTimeout(renderTimeout);
        renderTimeout = setTimeout(() => {
            renderBatchResults();
            needsRender = false;
            renderTimeout = null;
        }, 100);
    };

    const interval = setInterval(async () => {
        if (pending.size === 0) {
            clearInterval(interval);
            if (needsRender) renderBatchResults();
            return;
        }

        const checkIds = Array.from(pending);
        let hasUpdates = false;

        for (const taskId of checkIds) {
            try {
                const resp = await api.getTaskStatus(taskId);
                if (!resp.ok) {
                    const idx = taskMap[taskId];
                    if (idx !== undefined && batchResults[idx]) {
                        batchResults[idx].status = 'failed';
                        batchResults[idx].errorMsg = '任务不存在';
                        hasUpdates = true;
                    }
                    pending.delete(taskId);
                    continue;
                }
                const status = await resp.json();
                if (status.is_finished) {
                    const idx = taskMap[taskId];
                    if (idx !== undefined && batchResults[idx]) {
                        let newStatus;
                        if (status.is_success) {
                            newStatus = 'downloaded';
                        } else if (status.error) {
                            newStatus = 'failed';
                            batchResults[idx].errorMsg = status.error;
                        } else {
                            newStatus = 'failed';
                            batchResults[idx].errorMsg = 'download_failed';
                        }
                        if (batchResults[idx].status !== newStatus) {
                            batchResults[idx].status = newStatus;
                            hasUpdates = true;
                        }
                    }
                    pending.delete(taskId);
                }
            } catch (e) { /* skip */ }
        }

        if (hasUpdates) {
            needsRender = true;
            scheduleRender();
        }
    }, 2000);
}

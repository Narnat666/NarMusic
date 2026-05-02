import { api } from '../api.js';
import { store } from '../state.js';
import { formatBytes, formatDateTime, escapeHtml, showToast, parseFilenameFromContentDisposition, triggerDownload, streamDownloadWithProgress } from '../utils.js';
import { playMusicFromLibrary } from './player.js';

let musicLibrary = [];
let isLibraryLoading = false;
let isBatchMode = false;
let isBatchDownloading = false;
let batchDownloadAbortController = null;
let activeSingleDownloads = 0;
let selectedMusicIds = new Set();
let protectionEnabled = null; // null = 未加载, true/false = 已加载
let protectionToken = null;
let searchKeyword = '';
let activeFilter = 'all';

const LOCAL_DL_KEY = 'narmusic_dl_done';
function getDlDoneSet() { try { return new Set(JSON.parse(localStorage.getItem(LOCAL_DL_KEY) || '[]')); } catch { return new Set(); } }
function markDlDone(filename) { const s = getDlDoneSet(); s.add(filename); try { localStorage.setItem(LOCAL_DL_KEY, JSON.stringify([...s])); } catch {} }
function unmarkDlDone(filename) { const s = getDlDoneSet(); s.delete(filename); try { localStorage.setItem(LOCAL_DL_KEY, JSON.stringify([...s])); } catch {} }
function isDlDone(filename) { return getDlDoneSet().has(filename); }

export function initLibrary() {
    const refreshLibraryBtn = document.getElementById('refreshLibraryBtn');
    if (refreshLibraryBtn) {
        refreshLibraryBtn.addEventListener('click', () => {
            if (isLibraryDownloading() && !confirm('有文件正在下载，刷新列表将中断下载，确定继续？')) return;
            loadMusicLibrary();
        });
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

    const batchPlaylistBtn = document.getElementById('batchPlaylistBtn');
    if (batchPlaylistBtn) {
        batchPlaylistBtn.addEventListener('click', batchGeneratePlaylist);
    }

    // 搜索栏
    const searchInput = document.getElementById('librarySearchInput');
    const searchClear = document.getElementById('librarySearchClear');
    if (searchInput) {
        searchInput.addEventListener('input', () => {
            searchKeyword = searchInput.value.trim();
            if (searchClear) searchClear.style.display = searchKeyword ? 'flex' : 'none';
            renderMusicLibrary();
            if (isBatchMode) updateBatchUI();
        });
    }
    if (searchClear) {
        searchClear.addEventListener('click', () => {
            searchKeyword = '';
            searchInput.value = '';
            searchClear.style.display = 'none';
            renderMusicLibrary();
            if (isBatchMode) updateBatchUI();
        });
    }

    // 筛选按钮
    const filterChips = document.getElementById('libraryFilterChips');
    if (filterChips) {
        filterChips.addEventListener('click', (e) => {
            const chip = e.target.closest('.filter-chip');
            if (!chip) return;
            filterChips.querySelectorAll('.filter-chip').forEach(c => c.classList.remove('active'));
            chip.classList.add('active');
            activeFilter = chip.dataset.filter;
            renderMusicLibrary();
        });
    }

    document.addEventListener('tab:library', () => loadMusicLibrary());

    const libraryEl = document.getElementById('musicLibrary');
    if (libraryEl) {
        libraryEl.addEventListener('click', handleLibraryClick);
    }

    api.protectionStatus().then(data => {
        protectionEnabled = data.enabled;
        if (protectionEnabled) {
            protectionToken = localStorage.getItem('protection_token');
        }
    }).catch(() => {});
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
        downloadMusicFile(filename, target);
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
        musicLibrary = await api.libraryList();
        store.set('musicLibrary', musicLibrary);
        renderMusicLibrary();
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

function getFilteredLibrary() {
    let list = musicLibrary;

    // 模糊搜索
    if (searchKeyword) {
        const kw = searchKeyword.toLowerCase();
        list = list.filter(m => {
            const name = (m.custom_filename || m.system_filename || '').toLowerCase();
            return name.includes(kw);
        });
    }

    // 排序
    list = [...list];
    const getName = m => (m.custom_filename || m.system_filename || '').replace(/\.[^/.]+$/, '');

    // 中文按拼音排序，与英文统一排列
    // 将中文字符映射为拼音首字母，英文保持原样，统一用 en locale 排序
    // 拼音首字母表：用 localeCompare zh-CN 将汉字与 A-Z 锚点字比较来确定
    const _anchorChars = 'ABCDEFGHJKLMNOPQRSTWXYZ';
    const _anchorWords = '阿八擦搭鹅发噶哈鸡喀拉妈拿哦啪七然撒他挖西压杂';
    const _pinyinCache = {};
    function getPinyinInitial(ch) {
        const code = ch.charCodeAt(0);
        if (_pinyinCache[code] !== undefined) return _pinyinCache[code];
        // 用 localeCompare zh-CN 找到 ch 落在哪个拼音区间
        for (let i = 0; i < _anchorWords.length; i++) {
            if (ch.localeCompare(_anchorWords[i], 'zh-CN') <= 0) {
                // ch <= anchorWords[i]，则 ch 的拼音首字母为 anchorChars[i]
                // 但需要确认不是在前一个区间末尾
                if (i === 0 || ch.localeCompare(_anchorWords[i - 1], 'zh-CN') > 0) {
                    _pinyinCache[code] = _anchorChars[i];
                    return _anchorChars[i];
                }
            }
        }
        _pinyinCache[code] = 'Z';
        return 'Z';
    }
    function mixedCompare(a, b) {
        const ka = [...a].map(ch => {
            if (/[a-zA-Z]/.test(ch)) return ch.toUpperCase();
            if (/[\u4e00-\u9fff]/.test(ch)) return getPinyinInitial(ch);
            return ch;
        }).join('');
        const kb = [...b].map(ch => {
            if (/[a-zA-Z]/.test(ch)) return ch.toUpperCase();
            if (/[\u4e00-\u9fff]/.test(ch)) return getPinyinInitial(ch);
            return ch;
        }).join('');
        return ka.localeCompare(kb, 'en');
    }

    switch (activeFilter) {
        case 'name-asc':
            list.sort((a, b) => mixedCompare(getName(a), getName(b)));
            break;
        case 'name-desc':
            list.sort((a, b) => mixedCompare(getName(b), getName(a)));
            break;
        case 'time-new':
            list.sort((a, b) => (b.download_time || 0) - (a.download_time || 0));
            break;
        case 'time-old':
            list.sort((a, b) => (a.download_time || 0) - (b.download_time || 0));
            break;
        case 'size-large':
            list.sort((a, b) => (b.file_size || 0) - (a.file_size || 0));
            break;
        case 'size-small':
            list.sort((a, b) => (a.file_size || 0) - (b.file_size || 0));
            break;
    }

    return list;
}

function renderMusicLibrary() {
    const musicLibraryEl = document.getElementById('musicLibrary');
    const musicCountEl = document.getElementById('musicCount');

    const filteredList = getFilteredLibrary();

    if (!musicLibrary || musicLibrary.length === 0) {
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">library_music</span><div style="font-size: 16px; font-weight: 500;">音乐库为空</div><div style="font-size: 14px; margin-top: 8px;">前往下载页面添加音乐</div></div>';
        musicCountEl.textContent = '0';
        if (isBatchMode) toggleBatchMode();
        return;
    }

    musicCountEl.textContent = filteredList.length;

    if (filteredList.length === 0) {
        musicLibraryEl.innerHTML = '<div style="text-align: center; padding: 40px 20px; color: var(--md-sys-color-on-surface-variant);"><span class="material-symbols-rounded" style="font-size: 48px; margin-bottom: 16px; display: block;">search_off</span><div style="font-size: 16px; font-weight: 500;">未找到匹配的音乐</div><div style="font-size: 14px; margin-top: 8px;">尝试其他关键词或筛选条件</div></div>';
        return;
    }

    let html = '<div class="music-list-header"><span></span><span>歌曲</span><span class="col-center">延迟</span><span class="col-center">大小</span><span class="col-center">下载时间</span><span></span></div>';

    filteredList.forEach((music, index) => {
        const customFilename = music.custom_filename || '未知文件';
        const originalFilename = music.original_filename || '';
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

        html += '<div class="music-item' + (isSelected ? ' batch-selected' : '') + (isDlDone(systemFilename) ? ' downloaded' : '') + '" data-index="' + index + '" data-id="' + musicId + '">'
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
            + (isBatchMode ? '' : '<div class="music-actions-cell">'
            + '<button class="music-action-btn" title="下载" data-action="download" data-filename="' + escapeAttr(systemFilename) + '">'
            + '<span class="material-symbols-rounded">download</span>'
            + '</button>'
            + '<button class="music-action-btn" title="删除" data-action="delete" data-id="' + musicId + '" data-filename="' + escapeAttr(systemFilename) + '">'
            + '<span class="material-symbols-rounded">delete</span>'
            + '</button>'
            + '</div>')
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

async function downloadMusicFile(systemFilename, btnEl) {
    if (!systemFilename) { showToast('文件名无效', 'warning'); return; }

    const abortController = new AbortController();
    let originalContent = null;
    let cancelHandler = null;
    let progressEl = null;

    if (btnEl) {
        originalContent = btnEl.innerHTML;
        btnEl.innerHTML = '<span class="material-symbols-rounded">cancel</span> <span class="dl-progress" style="font-size:11px">0%</span>';
        btnEl.dataset.action = 'cancel-download';
        progressEl = btnEl.querySelector('.dl-progress');
        cancelHandler = (e) => {
            e.stopImmediatePropagation();
            abortController.abort();
        };
        btnEl.addEventListener('click', cancelHandler, true);
        const musicItem = btnEl.closest('.music-item');
        if (musicItem) {
            musicItem.classList.add('downloading');
            const deleteBtn = musicItem.querySelector('[data-action="delete"]');
            if (deleteBtn) deleteBtn.disabled = true;
        }
    }

    activeSingleDownloads++;
    try {
        const response = await api.downloadFileByName(systemFilename, abortController.signal);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let downloadFilename = systemFilename;
            const musicItem = musicLibrary.find(item => item.system_filename === systemFilename);
            if (musicItem) {
                if (musicItem.original_filename) {
                    downloadFilename = musicItem.original_filename;
                } else if (musicItem.custom_filename) {
                    downloadFilename = musicItem.custom_filename;
                }
            }
            if (contentDisposition) {
                downloadFilename = parseFilenameFromContentDisposition(contentDisposition, downloadFilename);
            }

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
                triggerDownload(blob, downloadFilename);
                markDlDone(systemFilename);
                const musicItem = btnEl ? btnEl.closest('.music-item') : null;
                if (musicItem) musicItem.classList.add('downloaded');
                showToast('文件下载完成', 'success');
            } else {
                unmarkDlDone(systemFilename);
                const musicItem = btnEl ? btnEl.closest('.music-item') : null;
                if (musicItem) musicItem.classList.remove('downloaded');
                showToast('下载已取消', 'info');
            }
        } else {
            const errorData = await response.json();
            showToast('下载未完成: ' + (errorData.error || '未知错误'), 'warning');
        }
    } catch (error) {
        if (abortController.signal.aborted) {
            unmarkDlDone(systemFilename);
            const musicItem = btnEl ? btnEl.closest('.music-item') : null;
            if (musicItem) musicItem.classList.remove('downloaded');
            showToast('下载已取消', 'info');
        } else {
            showToast('下载错误: ' + error.message, 'warning');
        }
    } finally {
        activeSingleDownloads--;
        if (btnEl) {
            if (cancelHandler) btnEl.removeEventListener('click', cancelHandler, true);
            btnEl.dataset.action = 'download';
            btnEl.innerHTML = originalContent;
            const musicItem = btnEl.closest('.music-item');
            if (musicItem) {
                musicItem.classList.remove('downloading');
                const deleteBtn = musicItem.querySelector('[data-action="delete"]');
                if (deleteBtn) deleteBtn.disabled = false;
            }
        }
    }
}

function ensureProtectionToken() {
    if (protectionEnabled === null) {
        return api.protectionStatus().then(data => {
            protectionEnabled = data.enabled;
            if (protectionEnabled && !protectionToken) {
                protectionToken = localStorage.getItem('protection_token');
            }
            return ensureProtectionToken();
        }).catch(() => {
            protectionEnabled = false;
            return null;
        });
    }
    if (!protectionEnabled) return Promise.resolve(null);
    if (protectionToken) return Promise.resolve(protectionToken);
    return new Promise((resolve) => {
        const overlay = document.createElement('div');
        overlay.style.cssText = 'position:fixed;top:0;left:0;width:100%;height:100%;background:rgba(0,0,0,0.5);z-index:10000;display:flex;align-items:center;justify-content:center;';
        const dialog = document.createElement('div');
        dialog.style.cssText = 'background:var(--md-sys-color-surface-container-high,#f0f0f0);border-radius:16px;padding:24px;max-width:320px;width:90%;box-shadow:0 4px 24px rgba(0,0,0,0.3);';
        dialog.innerHTML = '<div style="font-size:18px;font-weight:600;margin-bottom:8px;color:var(--md-sys-color-on-surface,#1d1b20);">文件保护</div>'
            + '<div style="font-size:14px;margin-bottom:16px;color:var(--md-sys-color-on-surface-variant,#49454f);">请输入密码以执行删除操作</div>'
            + '<div style="position:relative;display:flex;align-items:center;">'
            + '<input type="password" id="protectionPwdInput" style="width:100%;padding:10px 12px;padding-right:40px;border:1px solid var(--md-sys-color-outline,#79747e);border-radius:8px;font-size:16px;background:var(--md-sys-color-surface,#fef7ff);color:var(--md-sys-color-on-surface,#1d1b20);box-sizing:border-box;outline:none;" placeholder="输入密码" autofocus>'
            + '<button id="protectionTogglePwdBtn" type="button" style="position:absolute;right:8px;background:none;border:none;cursor:pointer;padding:4px;color:var(--md-sys-color-on-surface-variant,#49454f);display:flex;align-items:center;"><span class="material-symbols-rounded" style="font-size:20px;">visibility_off</span></button>'
            + '</div>'
            + '<div id="protectionPwdError" style="font-size:12px;color:var(--md-sys-color-error,#b3261e);margin-top:4px;display:none;">密码错误</div>'
            + '<div style="display:flex;gap:8px;justify-content:flex-end;margin-top:16px;">'
            + '<button id="protectionCancelBtn" style="padding:8px 16px;border:none;border-radius:8px;background:transparent;color:var(--md-sys-color-primary,#6750a4);font-size:14px;cursor:pointer;">取消</button>'
            + '<button id="protectionConfirmBtn" style="padding:8px 16px;border:none;border-radius:8px;background:var(--md-sys-color-primary,#6750a4);color:#fff;font-size:14px;cursor:pointer;">确认</button>'
            + '</div>';
        overlay.appendChild(dialog);
        document.body.appendChild(overlay);

        const input = dialog.querySelector('#protectionPwdInput');
        const errorEl = dialog.querySelector('#protectionPwdError');
        const confirmBtn = dialog.querySelector('#protectionConfirmBtn');
        const cancelBtn = dialog.querySelector('#protectionCancelBtn');
        const togglePwdBtn = dialog.querySelector('#protectionTogglePwdBtn');

        togglePwdBtn.addEventListener('click', () => {
            const isPassword = input.type === 'password';
            input.type = isPassword ? 'text' : 'password';
            togglePwdBtn.querySelector('.material-symbols-rounded').textContent = isPassword ? 'visibility' : 'visibility_off';
        });

        input.focus();

        const close = () => {
            document.body.removeChild(overlay);
            resolve(null);
        };

        const verify = async () => {
            const pwd = input.value.trim();
            if (!pwd) return;
            try {
                const data = await api.protectionVerify(pwd);
                if (data.valid && data.token) {
                    protectionToken = data.token;
                    localStorage.setItem('protection_token', protectionToken);
                    document.body.removeChild(overlay);
                    resolve(protectionToken);
                } else {
                    errorEl.style.display = 'block';
                    input.value = '';
                    input.focus();
                }
            } catch (e) {
                errorEl.style.display = 'block';
                input.value = '';
                input.focus();
            }
        };

        confirmBtn.addEventListener('click', verify);
        cancelBtn.addEventListener('click', close);
        input.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') verify();
            if (e.key === 'Escape') close();
        });
    });
}

async function deleteMusicFromLibrary(musicId, systemFilename) {
    if (!musicId) { showToast('文件ID无效', 'warning'); return; }
    const musicItem = musicLibrary.find(item => item.id === musicId);
    const displayName = musicItem
        ? (musicItem.custom_filename || musicItem.system_filename || '未知文件').replace(/\.[^/.]+$/, '')
        : (systemFilename || '该文件').replace(/\.[^/.]+$/, '');
    if (!confirm('确定要删除「' + displayName + '」吗？此操作不可恢复。')) return;

    const token = await ensureProtectionToken();
    if (protectionEnabled && !token) return;

    try {
        const data = await api.libraryDelete(musicId, token);
        if (data.success !== false) {
            showToast('已删除', 'success');
            loadMusicLibrary();
        } else {
            showToast('删除失败: ' + (data.error || '未知错误'), 'warning');
        }
    } catch (error) {
        if (error.code === 'protection_required') {
            protectionToken = null;
            localStorage.removeItem('protection_token');
            showToast('需要密码验证', 'warning');
        } else {
            showToast('删除错误: ' + error.message, 'warning');
        }
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
    const filteredIds = getFilteredLibrary().map(m => m.id);

    if (selectAllCheckbox.checked) {
        filteredIds.forEach(id => selectedMusicIds.add(id));
    } else {
        filteredIds.forEach(id => selectedMusicIds.delete(id));
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
    const batchPlaylistBtn = document.getElementById('batchPlaylistBtn');
    const selectAllCheckbox = document.getElementById('selectAllCheckbox');

    const count = selectedMusicIds.size;
    selectedCountEl.textContent = '已选 ' + count + ' 项';
    batchDownloadBtn.disabled = count === 0 && !isBatchDownloading;
    batchDeleteBtn.disabled = count === 0 || isBatchDownloading;
    batchPlaylistBtn.disabled = count === 0;

    const filteredIds = getFilteredLibrary().map(m => m.id);
    if (filteredIds.length > 0 && filteredIds.every(id => selectedMusicIds.has(id))) {
        selectAllCheckbox.checked = true;
    } else {
        selectAllCheckbox.checked = false;
    }
}

async function batchDownload() {
    if (isBatchDownloading) {
        if (batchDownloadAbortController) batchDownloadAbortController.abort();
        return;
    }

    const ids = Array.from(selectedMusicIds);
    if (ids.length === 0) { showToast('请先选择要下载的文件', 'warning'); return; }

    const batchDownloadBtn = document.getElementById('batchDownloadBtn');
    const batchDeleteBtn = document.getElementById('batchDeleteBtn');
    const originalDownloadContent = batchDownloadBtn.innerHTML;
    let batchProgressEl = null;

    const abortController = new AbortController();
    batchDownloadAbortController = abortController;
    isBatchDownloading = true;

    batchDeleteBtn.disabled = true;
    const batchSelectBtn = document.getElementById('batchSelectBtn');
    if (batchSelectBtn) batchSelectBtn.disabled = true;
    document.querySelectorAll('.music-action-btn[data-action="delete"], .music-action-btn[data-action="download"]').forEach(btn => btn.disabled = true);
    batchDownloadBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">hourglass_top</span> 准备中...';

    try {
        const response = await api.libraryBatchDownload(ids, abortController.signal);
        if (response.ok) {
            const contentDisposition = response.headers.get('Content-Disposition');
            let filename = 'NarMusic_download.zip';
            if (contentDisposition) {
                filename = parseFilenameFromContentDisposition(contentDisposition, filename);
            }

            batchDownloadBtn.innerHTML = '<span class="material-symbols-rounded">cancel</span> 下载中 <span class="dl-progress">0%</span>';
            batchDownloadBtn.title = '再次点击取消下载';
            batchProgressEl = batchDownloadBtn.querySelector('.dl-progress');

            const blob = await streamDownloadWithProgress(response, (received, total) => {
                if (abortController.signal.aborted) return;
                if (batchProgressEl) {
                    if (total > 0) {
                        const percent = Math.round((received / total) * 100);
                        batchProgressEl.textContent = percent + '%';
                    } else {
                        batchProgressEl.textContent = formatBytes(received);
                    }
                }
            }, abortController.signal);

            if (blob) {
                triggerDownload(blob, filename);
                ids.forEach(id => {
                    const item = musicLibrary.find(m => m.id === id);
                    if (item && item.system_filename) markDlDone(item.system_filename);
                });
                renderMusicLibrary();
                showToast((ids.length === 1 ? '文件' : ids.length + ' 个文件打包') + '下载已开始', 'success');
            } else {
                showToast('下载已取消', 'info');
            }
        } else {
            const errorData = await response.json();
            showToast('下载失败: ' + (errorData.message || '未知错误'), 'warning');
        }
    } catch (error) {
        if (abortController.signal.aborted) {
            showToast('下载已取消', 'info');
        } else {
            showToast('下载错误: ' + error.message, 'warning');
        }
    } finally {
        isBatchDownloading = false;
        batchDownloadAbortController = null;
        batchDownloadBtn.innerHTML = originalDownloadContent;
        batchDownloadBtn.title = '';
        batchDownloadBtn.disabled = selectedMusicIds.size === 0;
        batchDeleteBtn.disabled = selectedMusicIds.size === 0;
        const batchSelectBtn = document.getElementById('batchSelectBtn');
        if (batchSelectBtn) batchSelectBtn.disabled = false;
        document.querySelectorAll('.music-action-btn[data-action="delete"], .music-action-btn[data-action="download"]').forEach(btn => btn.disabled = false);
    }
}

async function batchDelete() {
    const ids = Array.from(selectedMusicIds);
    if (ids.length === 0) { showToast('请先选择要删除的文件', 'warning'); return; }
    if (!confirm('确定要删除选中的 ' + ids.length + ' 个文件吗？此操作不可恢复。')) return;

    const token = await ensureProtectionToken();
    if (protectionEnabled && !token) return;

    showToast('正在删除 ' + ids.length + ' 个文件...', 'info');

    const batchDeleteBtn = document.getElementById('batchDeleteBtn');
    batchDeleteBtn.disabled = true;
    const originalContent = batchDeleteBtn.innerHTML;
    batchDeleteBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">hourglass_top</span> 删除中...';

    try {
        const data = await api.libraryBatchDelete(ids, token);
        showToast('已删除 ' + (data.count || ids.length) + ' 个文件', 'success');
        selectedMusicIds.clear();
        loadMusicLibrary();
    } catch (error) {
        if (error.code === 'protection_required') {
            protectionToken = null;
            localStorage.removeItem('protection_token');
            showToast('需要密码验证', 'warning');
        } else {
            showToast('删除错误: ' + error.message, 'warning');
        }
    } finally {
        batchDeleteBtn.disabled = false;
        batchDeleteBtn.innerHTML = originalContent;
    }
}

async function batchGeneratePlaylist() {
    const ids = Array.from(selectedMusicIds);
    if (ids.length === 0) { showToast('请先选择要生成歌单的文件', 'warning'); return; }

    const batchPlaylistBtn = document.getElementById('batchPlaylistBtn');
    batchPlaylistBtn.disabled = true;
    const originalContent = batchPlaylistBtn.innerHTML;
    batchPlaylistBtn.innerHTML = '<span class="material-symbols-rounded" style="animation: spin 1s linear infinite;">hourglass_top</span> 生成中...';

    try {
        const data = await api.libraryGeneratePlaylist(ids);
        const playlist = data.playlist;
        const count = data.count || 0;

        const blob = new Blob([playlist], { type: 'text/plain;charset=utf-8' });
        const now = new Date();
        const dateStr = now.getFullYear()
            + String(now.getMonth() + 1).padStart(2, '0')
            + String(now.getDate()).padStart(2, '0') + '_'
            + String(now.getHours()).padStart(2, '0')
            + String(now.getMinutes()).padStart(2, '0')
            + String(now.getSeconds()).padStart(2, '0');
        const filename = 'NarMusic_playlist_' + dateStr + '.txt';
        triggerDownload(blob, filename);
        showToast('歌单已生成: ' + count + ' 条记录', 'success');
    } catch (error) {
        showToast('歌单生成失败: ' + error.message, 'warning');
    } finally {
        batchPlaylistBtn.disabled = selectedMusicIds.size === 0;
        batchPlaylistBtn.innerHTML = originalContent;
    }
}

export function isLibraryDownloading() {
    return isBatchDownloading || activeSingleDownloads > 0;
}

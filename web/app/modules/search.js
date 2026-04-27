import { api } from '../api.js';
import { showToast } from '../utils.js';
import { switchTabByName } from '../components/tabs.js';

let searchInput = null;
let searchIcon = null;
let isSearching = false;

export function initSearch() {
    searchInput = document.getElementById('searchInput');
    searchIcon = document.getElementById('searchIcon');

    if (!searchInput || !searchIcon) return;

    searchInput.addEventListener('keydown', (e) => {
        if (e.key === 'Enter') {
            e.preventDefault();
            e.stopPropagation();
            performSearch();
        }
    });
    searchInput.addEventListener('keyup', (e) => {
        if (e.key === 'Enter') { e.preventDefault(); }
    });
    searchIcon.addEventListener('click', performSearch);
    searchIcon.style.cursor = 'pointer';
}

async function performSearch() {
    const keyword = searchInput.value.trim();
    if (!keyword) { showToast('请输入搜索关键词', 'warning'); searchInput.focus(); return; }
    if (isSearching) return;

    isSearching = true;
    searchIcon.textContent = 'hourglass_top';
    searchIcon.style.animation = 'spin 1s linear infinite';
    searchInput.disabled = true;

    try {
        const response = await api.search(keyword);
        const data = await response.json();

        if (response.ok && data.link) {
            switchTabByName('download');
            const urlInput = document.getElementById('urlInput');
            const filenameInput = document.getElementById('filenameInput');
            if (urlInput) urlInput.value = data.title + ' ' + data.link;
            if (filenameInput) filenameInput.value = data.keyword;
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

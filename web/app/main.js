import { initTabs } from './components/tabs.js';
import { initDownload, stopPolling, isDownloading } from './modules/download.js';
import { initSearch } from './modules/search.js';
import { initLibrary, isLibraryDownloading } from './modules/library.js';
import { initBatch } from './modules/batch.js';
import { initPlayer } from './modules/player.js';
import { initSettings, loadSettings } from './modules/settings.js';
import { showToast } from './utils.js';

document.addEventListener('DOMContentLoaded', () => {
    loadSettings();
    initSettings();
    initTabs();
    initDownload();
    initSearch();
    initPlayer();
    initLibrary();
    initBatch();

    document.fonts.ready.then(() => {
        requestAnimationFrame(() => {
            requestAnimationFrame(() => {
                document.body.classList.remove('preload');
                document.body.style.visibility = '';
            });
        });
    });

    window.addEventListener('beforeunload', (e) => {
        if (isDownloading() || isLibraryDownloading()) {
            e.preventDefault();
            e.returnValue = '';
        }
    });

    window.addEventListener('unload', () => {
        stopPolling();
    });

    setTimeout(() => showToast('🎵 输入链接或包含链接的文本即可开始下载', 'info'), 1000);
});

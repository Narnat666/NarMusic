import { api } from '../api.js';
import { showToast } from '../utils.js';
import { fetchLyrics, parseLrcLyrics, displayLyrics, updateLyricsHighlight, setCurrentLyrics, initLyrics } from './lyrics.js';

let audioPlayer = null;
let audioSection = null;
let playerTitle = null;
let playerArtist = null;

export function initPlayer() {
    audioPlayer = document.getElementById('audioPlayer');
    audioSection = document.getElementById('audioSection');
    playerTitle = document.getElementById('playerTitle');
    playerArtist = document.getElementById('playerArtist');

    initLyrics();
    initDragFunctionality();

    const closeAudioBtn = document.getElementById('closeAudioBtn');
    if (closeAudioBtn) {
        closeAudioBtn.addEventListener('click', (e) => {
            e.stopPropagation();
            e.preventDefault();
            audioSection.classList.remove('show');
            audioPlayer.pause();
        });
    }

    if (audioPlayer) {
        audioPlayer.addEventListener('timeupdate', () => {
            updateLyricsHighlight(audioPlayer);
        });
    }
}

export async function playMusicFromLibrary(systemFilename, musicLibrary) {
    if (!systemFilename) {
        showToast('文件名无效', 'warning');
        return;
    }

    let musicItem = musicLibrary.find(item => item.system_filename === systemFilename);
    if (!musicItem) {
        musicItem = musicLibrary.find(item => item.custom_filename === systemFilename);
        if (!musicItem) {
            showToast('未找到对应的音乐文件', 'warning');
            return;
        }
    }

    if (!audioPlayer) initPlayer();

    const streamUrl = api.streamUrl(systemFilename, true);

    if (audioPlayer) {
        audioPlayer.src = streamUrl;
        audioPlayer.load();

        if (audioSection) {
            ensurePopupInViewport();
            audioSection.classList.add('show');
        }

        const customFilename = musicItem ? musicItem.custom_filename : systemFilename;
        const title = customFilename.replace(/\.[^/.]+$/, '') || '未知歌曲';
        if (playerTitle) playerTitle.textContent = title;
        if (playerArtist) playerArtist.textContent = '从音乐库播放';

        try {
            showToast('正在加载歌词...', 'info');
            const lyricsText = await fetchLyrics(systemFilename);
            if (lyricsText) {
                const lyrics = parseLrcLyrics(lyricsText);
                setCurrentLyrics(lyrics);
                displayLyrics(lyrics);
                showToast('歌词加载成功', 'success');
            } else {
                setCurrentLyrics([]);
                displayLyrics([]);
                showToast('未找到歌词', 'info');
            }
        } catch (error) {
            let errorMessage = '歌词加载失败';
            const errorMsg = error.message || String(error);
            if (errorMsg.includes('jsmediatags库未加载')) {
                errorMessage = '歌词库加载失败，请刷新页面';
            } else if (errorMsg.includes('HTTP')) {
                errorMessage = '服务器请求失败: ' + errorMsg;
            } else if (errorMsg.includes('Blob')) {
                errorMessage = '音频文件读取失败';
            } else if (errorMsg.includes("Offset") && errorMsg.includes("hasn't been loaded yet")) {
                errorMessage = '歌词数据不完整，尝试重新加载';
            }
            setCurrentLyrics([]);
            displayLyrics([]);
            showToast(errorMessage, 'warning');
        }

        setTimeout(() => {
            audioPlayer.play().catch(() => {
                showToast('点击播放按钮开始播放', 'info');
            });
        }, 500);

        showToast('正在加载音乐...', 'info');
    } else {
        showToast('音频播放器初始化失败', 'warning');
    }
}

function ensurePopupInViewport() {
    const audioPopup = document.getElementById('audioSection');
    if (!audioPopup) return;

    const viewportWidth = window.innerWidth;
    const viewportHeight = window.innerHeight;
    const popupWidth = audioPopup.offsetWidth;
    const popupHeight = audioPopup.offsetHeight;

    let currentLeft = parseInt(audioPopup.style.left) || 0;
    let currentTop = parseInt(audioPopup.style.top) || 0;

    if (!audioPopup.style.left || !audioPopup.style.top) {
        currentLeft = viewportWidth - popupWidth - 20;
        currentTop = viewportHeight - popupHeight - 20;
        audioPopup.style.left = currentLeft + 'px';
        audioPopup.style.top = currentTop + 'px';
        audioPopup.style.transform = 'translate(0, 0)';
    }

    let newLeft = Math.max(0, Math.min(currentLeft, viewportWidth - popupWidth));
    let newTop = Math.max(0, Math.min(currentTop, viewportHeight - popupHeight));

    if (newLeft !== currentLeft || newTop !== currentTop) {
        audioPopup.style.left = newLeft + 'px';
        audioPopup.style.top = newTop + 'px';
        audioPopup.style.transform = 'translate(0, 0)';
    }
}

function initDragFunctionality() {
    const audioPopup = document.getElementById('audioSection');
    const header = document.querySelector('.audio-popup-header');
    if (!audioPopup || !header) return;

    let isDragging = false;
    let xOffset = 0;
    let yOffset = 0;
    let lastTouchTime = 0;
    const TOUCH_DELAY = 16;

    header.addEventListener('mousedown', dragStart);
    header.addEventListener('touchstart', dragStart, { passive: false });
    document.addEventListener('mousemove', drag);
    document.addEventListener('touchmove', drag, { passive: false });
    document.addEventListener('mouseup', dragEnd);
    document.addEventListener('touchend', dragEnd);

    function dragStart(e) {
        if (e.target.closest('#closeAudioBtn')) return;

        let clientX, clientY;
        if (e.type === 'touchstart') {
            e.preventDefault();
            clientX = e.touches[0].clientX;
            clientY = e.touches[0].clientY;
        } else {
            clientX = e.clientX;
            clientY = e.clientY;
        }

        const rect = audioPopup.getBoundingClientRect();
        xOffset = clientX - rect.left;
        yOffset = clientY - rect.top;

        if (e.target === header || header.contains(e.target)) {
            isDragging = true;
            audioPopup.style.transition = 'none';
            header.style.cursor = 'grabbing';
        }
    }

    function drag(e) {
        if (!isDragging) return;
        e.preventDefault();

        if (e.type === 'touchmove') {
            const now = Date.now();
            if (now - lastTouchTime < TOUCH_DELAY) return;
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

        const viewportWidth = window.innerWidth;
        const viewportHeight = window.innerHeight;
        const popupWidth = audioPopup.offsetWidth;
        const popupHeight = audioPopup.offsetHeight;

        const boundedX = Math.max(0, Math.min(clientX - xOffset, viewportWidth - popupWidth));
        const boundedY = Math.max(0, Math.min(clientY - yOffset, viewportHeight - popupHeight));

        audioPopup.style.left = boundedX + 'px';
        audioPopup.style.top = boundedY + 'px';
        audioPopup.style.transform = 'translate(0, 0)';
    }

    function dragEnd() {
        if (!isDragging) return;
        isDragging = false;
        audioPopup.style.transition = 'transform 0.3s ease, opacity 0.3s ease';
        header.style.cursor = 'move';
        savePopupPosition();
    }

    function savePopupPosition() {
        const left = parseInt(audioPopup.style.left) || 0;
        const top = parseInt(audioPopup.style.top) || 0;
        localStorage.setItem('audioPopupPosition', JSON.stringify({ x: left, y: top }));
    }

    function loadPopupPosition() {
        const savedPosition = localStorage.getItem('audioPopupPosition');
        if (savedPosition) {
            const position = JSON.parse(savedPosition);
            const viewportWidth = window.innerWidth;
            const viewportHeight = window.innerHeight;
            const popupWidth = audioPopup.offsetWidth;
            const popupHeight = audioPopup.offsetHeight;
            let x = Math.max(0, Math.min(position.x, viewportWidth - popupWidth));
            let y = Math.max(0, Math.min(position.y, viewportHeight - popupHeight));
            audioPopup.style.top = y + 'px';
            audioPopup.style.left = x + 'px';
            audioPopup.style.transform = 'translate(0, 0)';
        }
    }

    loadPopupPosition();
    window.addEventListener('resize', () => loadPopupPosition());
}

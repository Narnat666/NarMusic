import { requestJson } from '../api.js';

let currentLyrics = [];
let lyricsDisplay = null;

export function initLyrics() {
    lyricsDisplay = document.getElementById('lyricsDisplay');
}

export async function fetchLyrics(filename) {
    try {
        const data = await requestJson('/api/lyrics?filename=' + encodeURIComponent(filename));
        if (data && data.lyrics) {
            return data.lyrics;
        }
        return null;
    } catch (error) {
        return null;
    }
}

export function parseLrcLyrics(lrcText) {
    const lines = lrcText.split('\n');
    const lyrics = [];
    const timeRegex = /\[(\d+):(\d+\.?\d*)\]/g;

    for (const line of lines) {
        const matches = [...line.matchAll(timeRegex)];
        if (matches.length === 0) continue;
        const text = line.replace(timeRegex, '').trim();
        if (!text) continue;
        for (const match of matches) {
            const minutes = parseFloat(match[1]);
            const seconds = parseFloat(match[2]);
            const time = minutes * 60 + seconds;
            lyrics.push({ time, text });
        }
    }

    lyrics.sort((a, b) => a.time - b.time);
    return lyrics;
}

export function displayLyrics(lyricsArray) {
    if (!lyricsDisplay) return;
    if (lyricsArray.length === 0) {
        lyricsDisplay.innerHTML = '<div class="lyrics-line">暂无歌词</div>';
        return;
    }
    let html = '';
    for (const item of lyricsArray) {
        const escapedText = _escapeAttr(String(item.time));
        const escapedContent = _escapeContent(item.text);
        html += '<div class="lyrics-line" data-time="' + escapedText + '">' + escapedContent + '</div>';
    }
    lyricsDisplay.innerHTML = html;
}

export function updateLyricsHighlight(audioPlayer) {
    if (!audioPlayer || currentLyrics.length === 0 || !lyricsDisplay) return;

    const currentTime = audioPlayer.currentTime;
    const lines = lyricsDisplay.querySelectorAll('.lyrics-line');

    let activeIndex = -1;
    for (let i = 0; i < currentLyrics.length; i++) {
        if (currentTime >= currentLyrics[i].time) {
            activeIndex = i;
        } else {
            break;
        }
    }

    lines.forEach(line => line.classList.remove('active'));
    if (activeIndex >= 0 && lines[activeIndex]) {
        lines[activeIndex].classList.add('active');
        lines[activeIndex].scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
}

export function setCurrentLyrics(lyrics) {
    currentLyrics = lyrics;
}

export function getCurrentLyrics() {
    return currentLyrics;
}

function _escapeContent(str) {
    const div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
}

function _escapeAttr(str) {
    return str.replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

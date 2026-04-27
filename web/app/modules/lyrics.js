import { api } from '../api.js';

let currentLyrics = [];
let lyricsDisplay = null;

export function initLyrics() {
    lyricsDisplay = document.getElementById('lyricsDisplay');
}

function fetchLyricsWithRange(filename, rangeStart, rangeEnd) {
    return new Promise((resolve, reject) => {
        api.streamWithRange(filename, rangeStart, rangeEnd)
            .then(response => {
                if (!response.ok) {
                    if (response.status === 416 && rangeStart !== null) {
                        return api.streamWithRange(filename, null, null);
                    }
                    throw new Error('HTTP ' + response.status + ' - ' + response.statusText);
                }
                return response.blob();
            })
            .then(blob => {
                if (!blob || blob.size === 0) {
                    reject(new Error('Blob为空或无效'));
                    return;
                }
                if (typeof jsmediatags === 'undefined') {
                    reject(new Error('jsmediatags库未加载'));
                    return;
                }
                jsmediatags.read(blob, {
                    onSuccess: function (tag) {
                        const lyrics = tag.tags.lyrics;
                        resolve(lyrics || null);
                    },
                    onError: function (error) {
                        reject(error);
                    }
                });
            })
            .catch(reject);
    });
}

export async function fetchLyrics(filename) {
    if (typeof jsmediatags === 'undefined') {
        throw new Error('jsmediatags库未加载');
    }

    try {
        const lyrics1 = await fetchLyricsWithRange(filename, 0, 524287);
        if (lyrics1 !== null) return lyrics1;
    } catch (e) { /* continue */ }

    try {
        let fileSize = null;
        try {
            const rangeResponse = await api.streamForSize(filename);
            const contentRange = rangeResponse.headers.get('Content-Range');
            if (contentRange) {
                const match = contentRange.match(/bytes \d+-\d+\/(\d+)/);
                if (match) fileSize = parseInt(match[1]);
            }
        } catch (e) { /* continue */ }

        if (fileSize && fileSize > 524288) {
            const rangeStart = Math.max(0, fileSize - 524288);
            const rangeEnd = fileSize - 1;
            const lyrics2 = await fetchLyricsWithRange(filename, rangeStart, rangeEnd);
            if (lyrics2 !== null) return lyrics2;
        } else if (fileSize) {
            const lyrics2 = await fetchLyricsWithRange(filename, null, null);
            if (lyrics2 !== null) return lyrics2;
        }
    } catch (e) { /* continue */ }

    try {
        const lyrics3 = await fetchLyricsWithRange(filename, null, null);
        if (lyrics3 !== null) return lyrics3;
        return null;
    } catch (error) {
        throw error;
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

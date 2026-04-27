import { store } from '../state.js';
import { showToast } from '../utils.js';

let platformSelect = null;
let delayInput = null;

export function initSettings() {
    platformSelect = document.getElementById('settingsPlatformSelect');
    delayInput = document.getElementById('settingsDelayInput');

    const saveBtn = document.getElementById('saveSettingsBtn');
    const resetBtn = document.getElementById('resetSettingsBtn');

    if (saveBtn) saveBtn.addEventListener('click', saveSettings);
    if (resetBtn) resetBtn.addEventListener('click', resetSettings);

    if (platformSelect) platformSelect.addEventListener('change', syncToStore);
    if (delayInput) delayInput.addEventListener('input', syncToStore);

    loadSettings();
}

export function loadSettings() {
    const savedPlatform = localStorage.getItem('musicPlatform');
    const savedDelay = localStorage.getItem('downloadDelay');

    if (savedPlatform && platformSelect) {
        platformSelect.value = savedPlatform;
    }
    if (savedDelay && delayInput) {
        delayInput.value = savedDelay;
    }

    syncToStore();
}

function saveSettings() {
    if (platformSelect) {
        localStorage.setItem('musicPlatform', platformSelect.value);
    }
    if (delayInput) {
        localStorage.setItem('downloadDelay', delayInput.value);
    }
    syncToStore();
    showToast('设置已保存', 'success');
}

function resetSettings() {
    if (platformSelect) platformSelect.value = '网易云音乐';
    if (delayInput) delayInput.value = '0';
    localStorage.removeItem('musicPlatform');
    localStorage.removeItem('downloadDelay');
    syncToStore();
    showToast('设置已恢复默认', 'info');
}

function syncToStore() {
    const platform = platformSelect ? platformSelect.value : '网易云音乐';
    let delayMs = 0;
    if (delayInput) {
        const delayText = delayInput.value.trim();
        if (delayText !== '') {
            const parsed = parseInt(delayText);
            if (!isNaN(parsed)) delayMs = parsed;
        }
    }
    store.set('settings', { platform, delayMs });
}

export function getSettingsValues() {
    const settings = store.get('settings');
    const platform = settings ? settings.platform : '网易云音乐';
    let delayMs = 0;
    let delayError = false;

    if (delayInput) {
        const delayText = delayInput.value.trim();
        if (delayText !== '') {
            const parsed = parseInt(delayText);
            if (isNaN(parsed)) {
                delayError = true;
            } else {
                delayMs = parsed;
            }
        }
    }

    return { platform, delayMs, delayError };
}

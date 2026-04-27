export function initTabs() {
    const sidebar = document.querySelector('.sidebar');
    if (!sidebar) return;

    sidebar.addEventListener('click', (e) => {
        const btn = e.target.closest('.nav-item');
        if (!btn) return;
        const tabName = btn.dataset.tab;
        if (!tabName) return;
        e.preventDefault();
        switchTab(tabName, btn);
    });
}

export function switchTab(tabName, activeBtn) {
    document.querySelectorAll('.tab-content').forEach(tab => tab.style.display = 'none');
    const tabEl = document.getElementById(tabName + 'Tab');
    if (tabEl) tabEl.style.display = 'block';

    document.querySelectorAll('.nav-item').forEach(item => item.classList.remove('active'));

    if (activeBtn) {
        activeBtn.classList.add('active');
    } else {
        const tabIndex = { download: 0, batchDownload: 1, library: 2, settings: 3 };
        const navItems = document.querySelectorAll('.nav-item');
        if (tabIndex[tabName] !== undefined && navItems[tabIndex[tabName]]) {
            navItems[tabIndex[tabName]].classList.add('active');
        }
    }

    if (tabName === 'library') {
        document.dispatchEvent(new CustomEvent('tab:library'));
    }
}

export function switchTabByName(tabName) {
    const tabIndex = { download: 0, batchDownload: 1, library: 2, settings: 3 };
    const navItems = document.querySelectorAll('.nav-item');
    const activeBtn = (tabIndex[tabName] !== undefined && navItems[tabIndex[tabName]]) ? navItems[tabIndex[tabName]] : null;
    switchTab(tabName, activeBtn);
}

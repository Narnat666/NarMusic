export const api = {
    createTask(payload) {
        return fetch('/api/message', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
    },

    getTaskStatus(taskId) {
        return fetch('/api/download/status?task_id=' + taskId);
    },

    downloadFileByTask(taskId) {
        return fetch('/api/download/file?task_id=' + taskId);
    },

    downloadFileByName(filename) {
        return fetch('/api/download/file?filename=' + encodeURIComponent(filename));
    },

    streamUrl(filename, withTimestamp = true) {
        let url = '/api/download/stream?filename=' + encodeURIComponent(filename);
        if (withTimestamp) url += '&t=' + Date.now();
        return url;
    },

    streamWithRange(filename, rangeStart, rangeEnd) {
        const url = '/api/download/stream?filename=' + encodeURIComponent(filename);
        const options = {};
        if (rangeStart !== null) {
            options.headers = { 'Range': 'bytes=' + rangeStart + '-' + rangeEnd };
        }
        return fetch(url, options);
    },

    streamForSize(filename) {
        const url = '/api/download/stream?filename=' + encodeURIComponent(filename);
        return fetch(url, { headers: { 'Range': 'bytes=0-0' } });
    },

    search(keyword) {
        return fetch('/api/search?keyword=' + encodeURIComponent(keyword));
    },

    batchSearch(keywords) {
        return fetch('/api/search/batch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ keywords })
        });
    },

    batchDownload(tasks, platform, offsetMs) {
        return fetch('/api/download/batch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tasks, platform, offsetMs })
        });
    },

    libraryList() {
        return fetch('/api/library/list');
    },

    libraryDelete(id) {
        return fetch('/api/library/delete?id=' + id, { method: 'DELETE' });
    },

    libraryBatchDelete(ids) {
        return fetch('/api/library/batch-delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ids })
        });
    },

    libraryBatchDownload(ids) {
        return fetch('/api/library/batch-download', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ids })
        });
    }
};

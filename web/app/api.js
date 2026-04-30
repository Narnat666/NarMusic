const DEFAULT_TIMEOUT = 15000;
const DEFAULT_RETRIES = 0;
const RETRY_DELAY = 1000;

class ApiError extends Error {
    constructor(status, code, message) {
        super(message);
        this.status = status;
        this.code = code;
    }
}

async function request(url, options = {}) {
    const {
        timeout = DEFAULT_TIMEOUT,
        retries = DEFAULT_RETRIES,
        signal: externalSignal,
        ...fetchOptions
    } = options;

    let lastError;
    for (let attempt = 0; attempt <= retries; attempt++) {
        const controller = new AbortController();
        const timer = timeout > 0 ? setTimeout(() => controller.abort(), timeout) : null;

        if (externalSignal) {
            if (externalSignal.aborted) {
                controller.abort();
            } else {
                externalSignal.addEventListener('abort', () => controller.abort(), { once: true });
            }
        }

        try {
            const response = await fetch(url, {
                ...fetchOptions,
                signal: controller.signal
            });
            if (timer) clearTimeout(timer);

            if (response.status === 429) {
                const retryAfter = parseInt(response.headers.get('Retry-After') || '5', 10);
                if (attempt < retries) {
                    await new Promise(r => setTimeout(r, retryAfter * 1000));
                    continue;
                }
            }

            if (!response.ok) {
                let code = 'unknown';
                let message = response.statusText;
                try {
                    const body = await response.json();
                    code = body.code || code;
                    message = body.message || body.msg || message;
                } catch {}
                throw new ApiError(response.status, code, message);
            }

            return response;
        } catch (err) {
            if (timer) clearTimeout(timer);
            if (err instanceof ApiError) throw err;
            if (err.name === 'AbortError') {
                lastError = new ApiError(0, 'timeout', '请求超时，请检查网络连接');
            } else {
                lastError = new ApiError(0, 'network', '网络错误，请检查连接');
            }
            if (attempt < retries) {
                await new Promise(r => setTimeout(r, RETRY_DELAY * (attempt + 1)));
            }
        }
    }
    throw lastError;
}

async function requestJson(url, options = {}) {
    const response = await request(url, options);
    return response.json();
}

export { ApiError, request, requestJson };

export const api = {
    createTask(payload) {
        return requestJson('/api/message', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload),
            retries: 1
        });
    },

    getTaskStatus(taskId) {
        return requestJson('/api/download/status?task_id=' + taskId);
    },

    batchGetTaskStatus(taskIds) {
        return requestJson('/api/download/status/batch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ task_ids: taskIds })
        });
    },

    downloadFileByTask(taskId, signal) {
        return request('/api/download/file?task_id=' + taskId, { timeout: 0, signal });
    },

    downloadFileByName(filename, signal) {
        return request('/api/download/file?filename=' + encodeURIComponent(filename), { timeout: 0, signal });
    },

    streamUrl(filename, withTimestamp = true) {
        let url = '/api/download/stream?filename=' + encodeURIComponent(filename);
        if (withTimestamp) url += '&t=' + Date.now();
        return url;
    },

    search(keyword) {
        return requestJson('/api/search?keyword=' + encodeURIComponent(keyword), {
            timeout: 20000,
            retries: 1
        });
    },

    batchSearch(keywords) {
        return requestJson('/api/search/batch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ keywords }),
            timeout: 30000,
            retries: 1
        });
    },

    batchDownload(tasks, platform, offsetMs) {
        return requestJson('/api/download/batch', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tasks, platform, offsetMs }),
            retries: 1
        });
    },

    libraryList() {
        return requestJson('/api/library/list');
    },

    libraryDelete(id) {
        return requestJson('/api/library/delete?id=' + id, { method: 'DELETE' });
    },

    libraryBatchDelete(ids) {
        return requestJson('/api/library/batch-delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ids })
        });
    },

    libraryBatchDownload(ids, signal) {
        return request('/api/library/batch-download', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ids }),
            timeout: 0,
            signal
        });
    }
};

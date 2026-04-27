export function createStore(initialState) {
    let state = { ...initialState };
    const listeners = new Map();

    return {
        get(key) { return state[key]; },
        getAll() { return { ...state }; },

        set(key, value) {
            const prev = state[key];
            state[key] = value;
            if (prev !== value && listeners.has(key)) {
                listeners.get(key).forEach(fn => fn(value, prev));
            }
        },

        subscribe(key, fn) {
            if (!listeners.has(key)) listeners.set(key, new Set());
            listeners.get(key).add(fn);
            return () => listeners.get(key).delete(fn);
        }
    };
}

export const store = createStore({
    musicLibrary: [],
    settings: {
        platform: '网易云音乐',
        delayMs: 0
    }
});

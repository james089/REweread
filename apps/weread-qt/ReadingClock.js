.pragma library

// Count observed foreground intervals, never wall time spent suspended.
function create() { return { last: 0, book: "", active: false, generation: -1, pending: {} } }
// Return only the interval accepted from the state before this call. The old
// book/generation are needed when a timer tick races a book or account change.
function sample(state, now, book, active, generation) {
    var delta = now - state.last
    var accepted = { book: "", ms: 0, endTimestamp: 0, generation: -1 }
    if (state.active && state.book && delta > 0 && delta <= 5000) {
        state.pending[state.book] = (state.pending[state.book] || 0) + delta
        accepted = { book: String(state.book), ms: delta, endTimestamp: now,
                     generation: Number(state.generation || 0) }
    }
    state.last = now
    state.book = String(book || "")
    state.active = !!active
    state.generation = Number(generation || 0)
    return accepted
}
function seconds(state, book) {
    return Math.floor((state.pending[book] || 0) / 1000)
}
function milliseconds(state, book) {
    return Math.max(0, Number(state.pending[book] || 0))
}
function restorePending(state, saved) {
    var restored = {}
    var values = saved || {}
    for (var book in values) {
        var value = Number(values[book] || 0)
        if (isFinite(value) && value > 0)
            restored[book] = Math.floor(value)
    }
    state.pending = restored
}
function acknowledge(state, book, secondsSent) {
    state.pending[book] = Math.max(0, (state.pending[book] || 0) - Math.max(0, secondsSent) * 1000)
}

/**
 * General-purpose display formatters.
 */

/**
 * Format a similarity / match-degree value to 2 decimal places.
 *
 * The backend returns `matchDegree` as a long floating-point number; the UI only
 * needs 2 decimals. The `-1` sentinel means "not applicable / no match" and is
 * rendered as empty (call sites also hide the field, but this keeps the helper
 * safe to reuse without that guard).
 *
 * @param {string|number|null|undefined} value
 * @returns {string}
 */
export const formatSimilarity = (value) => {
  if (value === null || value === undefined || value === '') return ''
  const num = Number(value)
  if (!Number.isFinite(num) || num === -1) return ''
  return num.toFixed(2)
}

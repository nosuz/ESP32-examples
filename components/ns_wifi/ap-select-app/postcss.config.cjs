const purgecss = require('@fullhuman/postcss-purgecss')

module.exports = {
  plugins: [
    purgecss({
      content: ['dist/*.html', 'dist/assets/*.js'],
      css: ['dist/assets/*.css'],
    })
  ]
}

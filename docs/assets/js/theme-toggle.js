document.addEventListener('DOMContentLoaded', function () {
  var saved = localStorage.getItem('jtd-theme');
  var current = saved === 'light' ? 'light' : 'dark';

  function applyTheme(theme) {
    current = theme;
    localStorage.setItem('jtd-theme', theme);
    var link = document.querySelector('link[rel="stylesheet"]');
    if (link) {
      var href = link.getAttribute('href');
      var next = href.replace(/just-the-docs-\w+\.css/, 'just-the-docs-' + theme + '.css');
      link.setAttribute('href', next);
    }
    document.querySelectorAll('.theme-toggle-btn').forEach(function (btn) {
      btn.textContent = theme === 'dark' ? 'View in Light Mode' : 'View in Dark Mode';
    });
  }

  if (saved === 'light') applyTheme('light');

  document.querySelectorAll('.theme-toggle-btn').forEach(function (btn) {
    btn.textContent = current === 'dark' ? 'View in Light Mode' : 'View in Dark Mode';
  });

  document.body.addEventListener('click', function (e) {
    if (e.target.classList.contains('theme-toggle-btn')) {
      applyTheme(current === 'dark' ? 'light' : 'dark');
    }
  });
});

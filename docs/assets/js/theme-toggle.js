// Part 1: 동기 실행 (head에서 즉시) — 렌더 전에 CSS 교체해서 flash 방지
(function () {
  var saved = localStorage.getItem('jtd-theme');
  if (saved === 'light' || saved === 'dark') {
    var link = document.querySelector('link[rel="stylesheet"]');
    if (link) {
      link.setAttribute(
        'href',
        link.getAttribute('href').replace(/just-the-docs-\w+\.css/, 'just-the-docs-' + saved + '.css')
      );
    }
  }
})();

// Part 2: DOM 준비 후 — 버튼 텍스트 + 클릭 핸들러 바인딩
document.addEventListener('DOMContentLoaded', function () {
  var saved = localStorage.getItem('jtd-theme');
  var current = saved === 'light' ? 'light' : 'dark';

  function applyTheme(theme) {
    current = theme;
    localStorage.setItem('jtd-theme', theme);
    var link = document.querySelector('link[rel="stylesheet"]');
    if (link) {
      link.setAttribute(
        'href',
        link.getAttribute('href').replace(/just-the-docs-\w+\.css/, 'just-the-docs-' + theme + '.css')
      );
    }
    document.querySelectorAll('.theme-toggle-btn').forEach(function (btn) {
      btn.textContent = theme === 'dark' ? 'View in Light Mode' : 'View in Dark Mode';
    });
  }

  // 버튼 초기 라벨
  document.querySelectorAll('.theme-toggle-btn').forEach(function (btn) {
    btn.textContent = current === 'dark' ? 'View in Light Mode' : 'View in Dark Mode';
  });

  // 이벤트 위임
  document.body.addEventListener('click', function (e) {
    if (e.target.classList.contains('theme-toggle-btn')) {
      applyTheme(current === 'dark' ? 'light' : 'dark');
    }
  });
});

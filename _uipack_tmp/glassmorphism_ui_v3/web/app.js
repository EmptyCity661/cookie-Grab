(() => {
  'use strict';
  const $ = (selector, root = document) => root.querySelector(selector);
  const $$ = (selector, root = document) => [...root.querySelectorAll(selector)];
  const viewNames = ['browser', 'settings', 'help'];
  let toastTimer;
  function showToast(message) {
    const toast = $('#toast');
    $('#toastText').textContent = message;
    toast.classList.add('visible');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => toast.classList.remove('visible'), 2200);
  }
  function showView(view) {
    if (!viewNames.includes(view)) return;
    $$('.nav-item').forEach(btn => btn.classList.toggle('active', btn.dataset.view === view));
    $$('.view').forEach(section => section.classList.toggle('active', section.id === `view-${view}`));
  }
  $$('.nav-item').forEach(btn => btn.addEventListener('click', () => showView(btn.dataset.view)));
  const status = $('#statusText');
  function demoAction(message) {
    status.textContent = message;
    showToast(message);
  }
  $('#openButton').addEventListener('click', () => {
    const value = $('#addressInput').value.trim();
    if (!value) { showToast('请先输入示例网址'); $('#addressInput').focus(); return; }
    try {
      const url = new URL(value.includes('://') ? value : `https://${value}`);
      if (!['http:', 'https:'].includes(url.protocol)) throw new Error('unsupported');
      $('#addressInput').value = url.href;
      demoAction(`演示模式：已验证地址格式 ${url.hostname}`);
    } catch { showToast('请输入有效的 HTTP 或 HTTPS 地址'); }
  });
  $('#refreshButton').addEventListener('click', () => demoAction('演示页面已刷新'));
  $('#miniRefresh').addEventListener('click', () => demoAction('示例页面已重新绘制'));
  $('#infoButton').addEventListener('click', () => showToast('这是本地 UI 原型，不会读取真实网页会话数据'));
  $('#demoExplore').addEventListener('click', () => showToast('这是用于展示布局的示例按钮'));
  $('#heroLearn').addEventListener('click', () => showToast('玻璃主题演示：背景模糊、透明层与细描边'));
  $('#glassLevel').addEventListener('input', event => {
    const value = Number(event.target.value);
    document.documentElement.style.setProperty('--blur', `${value === 1 ? 12 : value === 2 ? 24 : 34}px`);
    $('#glassLevelText').textContent = ['轻柔', '标准', '浓郁'][value - 1];
  });
  $$('.segmented button').forEach(btn => btn.addEventListener('click', () => {
    $$('.segmented button').forEach(item => item.classList.toggle('selected', item === btn));
    document.body.classList.toggle('light-glass', btn.dataset.theme === 'light');
    if (btn.dataset.theme === 'system') {
      document.body.classList.toggle('light-glass', window.matchMedia('(prefers-color-scheme: light)').matches);
    }
    showToast(`已预览${btn.textContent.trim()}主题`);
  }));
  $('#saveSettings').addEventListener('click', () => {
    const settings = {
      launchMode: $('#launchMode').value,
      defaultUrl: $('#defaultUrl').value,
      logName: $('#logName').value,
      density: $('#density').value,
      glassLevel: $('#glassLevel').value,
      switches: $$('.switch').map(el => el.checked),
      theme: $('.segmented .selected')?.dataset.theme || 'dark'
    };
    try { localStorage.setItem('glass-ui-demo-settings', JSON.stringify(settings)); } catch { /* local storage can be unavailable for file:// pages */ }
    showToast('演示设置已保存到本地页面存储');
  });
  $('#resetSettings').addEventListener('click', () => {
    $('#launchMode').value = '自定义示例页面';
    $('#defaultUrl').value = 'https://example.com';
    $('#logName').value = 'browser-workspace.log';
    $('#density').value = '舒适';
    $('#glassLevel').value = '2';
    $('#glassLevel').dispatchEvent(new Event('input'));
    $$('.switch').forEach((el, i) => el.checked = [true, true, false, true][i]);
    $$('.segmented button').forEach(btn => btn.classList.toggle('selected', btn.dataset.theme === 'dark'));
    document.body.classList.remove('light-glass');
    showToast('演示设置已恢复默认值');
  });
  $('#copyPath').addEventListener('click', async () => {
    const text = $('#dataPath').value;
    try { await navigator.clipboard.writeText(text); showToast('示例目录路径已复制'); }
    catch { $('#dataPath').focus(); $('#dataPath').select(); showToast('请手动复制选中的示例路径'); }
  });
  window.addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === '1') showView('browser');
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === '2') showView('settings');
  });
})();

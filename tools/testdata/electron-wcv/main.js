// mOS — teste de composição de WebContentsView no Electron, no formato da janela do Claude desktop: uma
// BrowserWindow oculta com fundo #fcfcfb e a própria página embaixo (o "shell"), e uma WebContentsView
// transparente por cima (o claude.ai) com um quadrado vermelho. Se a janela mostrar o quadrado vermelho com o
// texto, várias WebContentsView na mesma janela se compõem certo no mOS.
//   .\tools\vscode-teste.ps1 -Electron -App "build\apps\x\cursor\Cursor.app" -Nome Cursor -OutDir build-electron
//       -SubstituirApp tools\testdata\electron-wcv
// Também testa URLs entre apps (userland/libsystem/openurl.c): 15 s depois de mostrar a janela chama
// shell.openExternal('https://example.com/...') (vai ao navegador da imagem: o Chrome ou o navegador de teste de
// tools\testdata\navegador-teste, com -ComApp) e registra cada 'open-url' que chegar (o esquema do app, ex.: cursor://,
// mandado por -AbrirURL).
const { app, BrowserWindow, WebContentsView, shell } = require('electron');

const pagina = (corpo) => 'data:text/html;charset=utf-8,' + encodeURIComponent(corpo);
const curta = (u) => { const m = /^([a-z0-9+.-]+:\/\/[^/?#]*)/i.exec(u); return m ? m[1] + '/…' : u.slice(0, 40); };
app.on('open-url', (e, url) => { e.preventDefault(); console.log('[teste-wcv] open-url ' + curta(url)); });

app.whenReady().then(() => {
  const win = new BrowserWindow({ width: 600, height: 600, show: false, backgroundColor: '#fcfcfb', titleBarStyle: 'hidden' });
  win.loadURL(pagina('<body style="margin:0;background:#fcfcfb;font:22px sans-serif">'
    + '<p style="position:absolute;left:20px;bottom:10px;margin:0;color:#444">shell (a página da janela, embaixo)</p></body>'));

  const topo = new WebContentsView();
  topo.setBackgroundColor('#00000000');
  win.contentView.addChildView(topo);
  const ajusta = () => { const b = win.getContentBounds(); topo.setBounds({ x: 0, y: 0, width: b.width, height: b.height - 60 }); };
  ajusta();
  win.on('resize', ajusta);
  topo.webContents.loadURL(pagina('<body style="margin:0;background:transparent">'
    + '<div style="margin:120px auto 0;width:320px;height:180px;background:#d00;color:#fff;font:30px sans-serif;'
    + 'display:flex;align-items:center;justify-content:center">view de cima</div></body>'));

  win.once('ready-to-show', () => {
    win.show();
    console.log('[teste-wcv] janela mostrada');
    setTimeout(() => shell.openExternal('https://example.com/abrir-url-teste?token=segredo').then(
      () => console.log('[teste-wcv] openExternal: ok'), (err) => console.log('[teste-wcv] openExternal falhou: ' + err)), 15000);
  });
  topo.webContents.on('did-finish-load', () => console.log('[teste-wcv] view de cima carregada'));
  setInterval(() => console.log('[teste-wcv] vivo'), 30000);
});
app.on('window-all-closed', () => app.quit());

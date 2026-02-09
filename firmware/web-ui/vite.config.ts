import { defineConfig } from 'vite';
import tailwindcss from '@tailwindcss/vite';
import { viteSingleFile } from 'vite-plugin-singlefile';

export default defineConfig({
  root: 'src',
  plugins: [
    tailwindcss(),
    viteSingleFile(),
  ],
  build: {
    outDir: '../dist',
    emptyOutDir: true,
    minify: 'terser',
    // Single file output — all CSS/JS inlined into index.html
  },
});

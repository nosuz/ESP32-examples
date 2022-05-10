import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import viteCompression from "vite-plugin-compression";
import purgecss from '@fullhuman/postcss-purgecss'

// https://vitejs.dev/config/
export default defineConfig(({ command, mode }) => {
  // console.log(command, mode)
  if (mode === "production") {
    return {
      plugins: [svelte(), viteCompression()],
      css: {
        postcss: {
          plugins: [
                purgecss({
                  content: ['dist/*.html', 'dist/assets/*.js'],
                  css: ['dist/assets/*.css'],
                  safelist: [/filepond-*/],
                })
          ]
        }
      }
    };
  } else {
    return {
      plugins: [svelte()],
        server: {
          proxy: {
            '/api':{
              target: 'http://esp32.local:80/',
              changeOrigin: true,
              secure: false,
            }
          }
        }
    };
  }
});

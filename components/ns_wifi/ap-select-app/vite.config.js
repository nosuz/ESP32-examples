import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";
import viteCompression from 'vite-plugin-compression';

// https://vitejs.dev/config/
export default defineConfig(({command, mode}) => {
  // console.log(command, mode)
  if (mode == "development") {
    return {
      plugins: [svelte()],

      css: {
        preprocessorOptions: {
          scss: {
            additionalData: '@use "src/variables.scss" as *;',
          },
        },
      },

      server: {
        proxy: {
          '/api':{
            target: 'http://esp32.local:80/',
            changeOrigin: true,
            secure: false,
          }
        }
      }
    }
  } else {
    return {
      plugins: [svelte(), viteCompression()],

      css: {
        preprocessorOptions: {
          scss: {
            additionalData: '@use "src/variables.scss" as *;',
          },
        },
      },
    }
  }
})

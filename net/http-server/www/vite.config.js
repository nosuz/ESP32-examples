import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import viteCompression from 'vite-plugin-compression';

// https://vitejs.dev/config/
export default defineConfig(({command, mode}) => {
  // console.log(command, mode)
  if (mode == "development") {
    return {
      plugins: [vue()],
      server: {
        proxy: {
          // '/api/led_on': 'http://localhost:8888/',
          '/api':{
            target: 'http://esp32.local:80/',
            // target: 'http://10.0.0.10:80/',
            // target: 'http://localhost:8888/',
            changeOrigin: true,
            secure: false,
          }
        }
      }
    }
  } else {
    return {
      plugins: [vue(), viteCompression()],
    }
  }
})

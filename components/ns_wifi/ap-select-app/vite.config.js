import { defineConfig } from "vite";
import { svelte } from "@sveltejs/vite-plugin-svelte";

// https://vitejs.dev/config/
export default defineConfig(({command, mode}) => {
  // console.log(command, mode)
  if (mode == "development") {
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
    }
  } else {
    return {
      plugins: [svelte()],
    }
  }
})

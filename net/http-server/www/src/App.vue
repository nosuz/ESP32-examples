<script>
import axios from "axios";

export default {
  data() {
    return {
      led_status: false,
      timer: null,
      next_status: true,
    };
  },
  mounted() {
    this.timer = setInterval(this.get_led, 1000);
  },
  beforeUnmount() {
    this.stopTimer();
  },
  methods: {
    set_led_on() {
      console.log("ON");
      this.set_led(true);
    },
    set_led_off() {
      console.log("OFF");
      this.set_led(false);
    },
    toggle_led() {
      if (this.led_status) {
        this.set_led(false)
      } else {
        this.set_led(true)
      }
    },
    set_led(state) {
      console.log(state);
      let end_point;
      if (state) {
        end_point = "/api/led_on"
      } else {
        end_point = "/api/led_off"
      }
      this.led_status = state;

      axios
        .get(end_point)
        // .then((response) => console.log(response))
        .then((response) => {
          console.log("set status: " + state);
          this.led_status = state;
          })
        .catch((error) => console.log(error));
    },
    get_led() {
      if (document.visibilityState === 'visible') {
        axios
          .get("/api/led_status")
          .then((response) => {
            console.log(response.data);
            this.led_status = response.data.led_status;
            })
          .catch((error) => console.log(error));
      // } else {
      //   console.log("skip");
      }
    },
    stopTimer() {
      clearInterval(this.timer);
    }
  },
  computed: {
    button_class() {
      if (this.led_status) {
        return "btn-success";
      } else {
        return "btn-outline-success";
      }
    },
  }
};
</script>

<template>
  <div class="container">
    <h2>LED Status</h2>
    <div class="row  mb-3  justify-content-center p-3">
      <div class="col">
        <p class="fw-bold"><span class="border border-dark border-1 rounded-pill p-3" :class="[this.led_status ? 'bg-success text-white' : '']">LED port: {{ led_status }}</span></p>
      </div>
    </div>
    <div class="row  mb-3  justify-content-center">
      <button type="button" class="btn col-sm-10 col-lg-6" :class="button_class" @click="toggle_led">Toggle</button>
    </div>
    <div class="row justify-content-center">
      <button
        type="button"
        class="btn btn-outline-success mx-1 col-sm-5 col-lg-3"
        :disabled="led_status"
        @click="set_led_on"
      >
        ON
      </button>
      <button
        type="button"
        class="btn btn-outline-secondary mx-1 col-sm-5 col-lg-3"
        :disabled="!led_status"
        @click="set_led_off"
      >
        OFF
      </button>
    </div>
  </div>
</template>

<style></style>

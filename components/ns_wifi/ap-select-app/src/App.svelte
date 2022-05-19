<script>
  import { onMount } from "svelte";
  import axios from "axios";
  import { Toast } from "bootstrap";

  let selected_ap;
  let ap_password = "";
  let ap_list = [];
  let form_password_view = false;

  $: togglePassFieldType(form_password_view);
  function togglePassFieldType(form_password_view) {
    let password_inout = document.querySelector("#password");
    if (password_inout) {
      password_inout.type = form_password_view ? "text" : "password";
    }
  }

  function handleSubmit() {
    console.log("SSID: " + selected_ap);

    try {
      // send application/x-www-form-urlencoded
      let params = new URLSearchParams();
      params.append("ssid", selected_ap);
      params.append("password", ap_password);
      const res = axios.post("/api/ap_config", params);

      // const res = axios.post("/api/ap_config", {
      //   ssid: selected_ap,
      //   password: ap_password
      // });
      console.log(res);

      let okToast = new Toast(document.getElementById("postOkToast"), {});
      okToast.show();
    } catch (e) {
      console.log(e);
    }
  }

  onMount(async () => {
    try {
      const res = await axios.get("/api/ap_list");
      ap_list = res.data;
      console.log(ap_list);
      if (ap_list && ap_list.length > 0) {
        selected_ap = ap_list[0].ssid;
      }
    } catch (e) {
      console.log(e);
    }
  });
</script>

<main class="container">
  <form on:submit|preventDefault={handleSubmit}>
    <div class="row my-3">
      <label for="ssid" class="col-sm-2 col-form-label">Access Point</label>
      <select
        class="col-sm-10 form-select"
        aria-label="select SSID"
        id="ssid"
        bind:value={selected_ap}
      >
        {#each ap_list as ap}
          <option value={ap.ssid}>{ap.ssid}</option>
        {/each}
      </select>
    </div>

    <div class="row mb-3">
      <label for="password" class="col-sm-2 col-form-label">Passoword</label>
      <input
        class="col-sm-10 form-control"
        id="password"
        type="password"
        bind:value={ap_password}
        placeholder="Access Point password"
      />
    </div>

    <div class="row mb-3">
      <div class="col-sm-5 form-check">
        <input
          class="form-check-input ms-1"
          type="checkbox"
          bind:checked={form_password_view}
          id="toggleVisiblity"
        />
        <label class="form-check-label ms-2" for="toggleVisiblity">
          Show password
        </label>
      </div>
    </div>

    <div class="row">
      <div class="col-sm-12 text-end">
        <button type="submit" class="btn btn-primary" disabled={!selected_ap}
          >Connect</button
        >
      </div>
    </div>
  </form>

  <!-- Toast -->
  <div class="position-absolute p-3 top-0 start-50  translate-middle-x">
    <div class="toast" id="postOkToast">
      <div class="toast-header">
        <strong class="me-auto">Updated AP params</strong>
        <button
          type="button"
          class="btn-close"
          data-bs-dismiss="toast"
          aria-label="Close"
        />
      </div>
      <div class="toast-body">
        Connecting to {selected_ap}
      </div>
    </div>
  </div>
</main>

<style>
  @import "bootstrap/dist/css/bootstrap.min.css";
</style>

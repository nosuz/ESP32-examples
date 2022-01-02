#!/usr/bin/python3

from flask import Flask, jsonify, Response, send_from_directory
# from flask_cors import CORS

import random

app = Flask(__name__)
# CORS(app)

led_status = False


@app.route("/api/led_on")
def set_led_on():
    global led_status
    led_status = True
    return Response(status=200)


@app.route("/api/led_off")
def set_led_off():
    global led_status
    led_status = False
    return Response(status=200)


@app.route("/api/led_status")
def get_led():
    # led = {"led_status": True if random.randint(0, 9) >= 5 else False}
    global led_status
    led = {"led_status": led_status}
    return jsonify(led)


if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0", port=8888)

#version 300 es

precision mediump float;

flat in int tag;
out int out_tag;

void main() {
  out_tag = tag;
}

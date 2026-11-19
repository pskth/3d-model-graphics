(() => {
  const wasmUrl = new URL('lumina.wasm', document.baseURI);

  async function loadLumina() {
    const response = await fetch(wasmUrl);
    if (!response.ok) throw new Error(`Failed to load lumina.wasm: ${response.status}`);
    let result;
    if (WebAssembly.instantiateStreaming) {
      result = await WebAssembly.instantiateStreaming(response, { env: {} });
    } else {
      result = await WebAssembly.instantiate(await response.arrayBuffer(), { env: {} });
    }
    return result.instance.exports;
  }

  window.LuminaC = {
    ready: loadLumina().catch((error) => {
      console.error(error);
      throw error;
    }),
  };
})();

# A tiny test model for Blitzen's QEMU test: a 2-layer llama-architecture GGUF with random weights (about 17 MB)
# and the llama SPM vocabulary that ships with llama.cpp's sources. It talks nonsense, but it loads, streams and
# exercises everything around the model: download, signature, engine, chat streaming, idle unload.
#   python3 mktinymodel.py LLAMA_CPP_DIR OUT.gguf          GPL-2.0-or-later
import sys, numpy as np
src, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, src + '/gguf-py')
import gguf
voc = gguf.GGUFReader(src + '/models/ggml-vocab-llama-spm.gguf')
w = gguf.GGUFWriter(out, 'llama')
E, L, H, F = 128, 2, 4, 256
w.add_name('Blitzen test model (random weights)')
w.add_context_length(2048); w.add_embedding_length(E); w.add_block_count(L); w.add_feed_forward_length(F)
w.add_head_count(H); w.add_head_count_kv(H); w.add_rope_dimension_count(E // H); w.add_layer_norm_rms_eps(1e-5)
w.add_file_type(gguf.LlamaFileType.MOSTLY_F16)
for name, f in voc.fields.items():
    if not name.startswith('tokenizer.'): continue
    vt = f.types[0]
    if vt == gguf.GGUFValueType.ARRAY:
        et = f.types[1]; vals = f.contents()
        w.add_key_value(name, vals, vt, sub_type=et)
    else:
        w.add_key_value(name, f.contents(), vt)
w.add_chat_template("{% for m in messages %}<|{{ m['role'] }}|>\n{{ m['content'] }}\n{% endfor %}{% if add_generation_prompt %}<|assistant|>\n{% endif %}")
V = len(voc.fields['tokenizer.ggml.tokens'].contents())
rng = np.random.default_rng(7)
t = lambda *s: (rng.standard_normal(s) * 0.02).astype(np.float16)
w.add_tensor('token_embd.weight', t(V, E)); w.add_tensor('output_norm.weight', np.ones(E, np.float32)); w.add_tensor('output.weight', t(V, E))
for i in range(L):
    p = f'blk.{i}.'
    w.add_tensor(p + 'attn_norm.weight', np.ones(E, np.float32)); w.add_tensor(p + 'ffn_norm.weight', np.ones(E, np.float32))
    for n in ('attn_q', 'attn_k', 'attn_v', 'attn_output'): w.add_tensor(p + n + '.weight', t(E, E))
    w.add_tensor(p + 'ffn_gate.weight', t(F, E)); w.add_tensor(p + 'ffn_up.weight', t(F, E)); w.add_tensor(p + 'ffn_down.weight', t(E, F))
w.write_header_to_file(); w.write_kv_data_to_file(); w.write_tensors_to_file(); w.close()
print(out, V, 'tokens')

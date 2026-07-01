(function () {
  'use strict';

  let ws = null;
  let reconnectTimer = null;
  let selectedMem = 1;
  let suppressSegEdit = false;
  let units = 'kph';

  const $ = (id) => document.getElementById(id);

  function wsUrl() {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return proto + '//' + location.host + '/ws';
  }

  function send(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(obj));
    }
  }

  function formatDist(m) {
    m = Number(m) || 0;
    if (Math.abs(m) > 999999) return Math.round(m / 1000).toLocaleString() + ' km';
    return Math.round(m).toLocaleString() + ' m';
  }

  function applyTelemetry(msg) {
    $('rally-clock').textContent = msg.rally_clock || '--:--:--';
    const ab = Number(msg.ahead_behind_s) || 0;
    $('ahead-value').textContent = (ab >= 0 ? '+' : '') + ab.toFixed(1) + ' s';
    const abEl = $('ahead-behind');
    abEl.classList.toggle('ahead', ab > 0.05);
    abEl.classList.toggle('behind', ab < -0.05);

    units = msg.units || 'kph';
    $('unit-label').textContent = units;
    $('unit-label2').textContent = units;
    $('cur-kph').textContent = Number(msg.cur_kph || 0).toFixed(1);
    $('target-kph').textContent = Number(msg.target_kph || 0).toFixed(1);
    $('trip-m').textContent = formatDist(msg.trip_m);
    $('total-m').textContent = formatDist(msg.total_m);
    $('trip-avg').textContent = 'avg ' + Number(msg.trip_avg_kph || 0).toFixed(1);
    $('total-avg').textContent = 'avg ' + Number(msg.total_avg_kph || 0).toFixed(1);
    $('seg-num').textContent = msg.segment_number || 0;
    $('seg-count').textContent = msg.segment_count || 0;

    const btn = $('btn-next-prev');
    btn.textContent = msg.next_prev_label || '--->';
    btn.disabled = !msg.next_prev_enabled;
  }

  function renderSegments(state) {
    const tbody = $('seg-table').querySelector('tbody');
    tbody.innerHTML = '';
    suppressSegEdit = true;
    (state.segments || []).forEach((seg, i) => {
      const tr = document.createElement('tr');
      tr.innerHTML =
        '<td>' + (i + 1) + '</td>' +
        '<td><input type="number" step="0.1" class="spd" value="' + seg.target_speed_kph + '"></td>' +
        '<td><input type="number" step="1" class="dist" value="' + seg.distance_m + '"></td>' +
        '<td><input type="checkbox" class="auto"' + (seg.autoNext ? ' checked' : '') + '></td>' +
        '<td><button type="button" class="del">del</button></td>';
      tr.querySelector('.spd').addEventListener('change', () => pushSegSet(i, tr));
      tr.querySelector('.dist').addEventListener('change', () => pushSegSet(i, tr));
      tr.querySelector('.auto').addEventListener('change', () => pushSegSet(i, tr));
      tr.querySelector('.del').addEventListener('click', () => send({ type: 'segment_delete', index: i }));
      tbody.appendChild(tr);
    });
    suppressSegEdit = false;
  }

  function pushSegSet(index, tr) {
    if (suppressSegEdit) return;
    send({
      type: 'segment_set',
      index: index,
      target_speed_kph: Number(tr.querySelector('.spd').value),
      distance_m: Number(tr.querySelector('.dist').value),
      autoNext: tr.querySelector('.auto').checked
    });
  }

  function onMessage(ev) {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (msg.type === 'telemetry') applyTelemetry(msg);
    if (msg.type === 'state') renderSegments(msg);
  }

  function connect() {
    if (ws) { ws.onclose = null; ws.close(); }
    ws = new WebSocket(wsUrl());
    $('conn').textContent = 'connecting…';
    $('conn').classList.remove('ok');
    ws.onopen = () => {
      $('conn').textContent = 'connected';
      $('conn').classList.add('ok');
    };
    ws.onclose = () => {
      $('conn').textContent = 'disconnected';
      $('conn').classList.remove('ok');
      clearTimeout(reconnectTimer);
      reconnectTimer = setTimeout(connect, 2000);
    };
    ws.onmessage = onMessage;
  }

  $('tab-live').addEventListener('click', () => {
    $('tab-live').classList.add('active');
    $('tab-setup').classList.remove('active');
    $('view-live').classList.remove('hidden');
    $('view-setup').classList.add('hidden');
  });
  $('tab-setup').addEventListener('click', () => {
    $('tab-setup').classList.add('active');
    $('tab-live').classList.remove('active');
    $('view-setup').classList.remove('hidden');
    $('view-live').classList.add('hidden');
  });

  $('btn-next-prev').addEventListener('click', () => send({ type: 'next_prev' }));
  $('btn-reset-trip').addEventListener('click', resetTrip);
  $('btn-reset-trip2').addEventListener('click', resetTrip);
  $('btn-reset-total').addEventListener('click', () => {
    if (confirm('Reset total distance?')) send({ type: 'reset_total' });
  });

  function resetTrip() {
    if (confirm('Reset trip distance?')) send({ type: 'reset_trip' });
  }

  $('btn-add-seg').addEventListener('click', () => {
    const kph = prompt('Target speed (kph):', '75');
    const dist = prompt('Distance (m):', '1000');
    if (!kph || !dist) return;
    send({
      type: 'segment_add',
      target_speed_kph: Number(kph),
      distance_m: Number(dist),
      autoNext: true
    });
  });

  document.querySelectorAll('.mem-slot').forEach((btn) => {
    btn.addEventListener('click', () => {
      selectedMem = Number(btn.dataset.slot);
      document.querySelectorAll('.mem-slot').forEach((b) => b.classList.remove('selected'));
      btn.classList.add('selected');
    });
  });
  document.querySelector('.mem-slot[data-slot="1"]').classList.add('selected');

  $('btn-mem-store').addEventListener('click', () => {
    if (confirm('Store current segments to memory ' + selectedMem + '?')) {
      send({ type: 'memory_store', slot: selectedMem });
    }
  });
  $('btn-mem-recall').addEventListener('click', () => {
    if (confirm('Recall memory ' + selectedMem + '?')) {
      send({ type: 'memory_recall', slot: selectedMem });
    }
  });

  connect();
})();

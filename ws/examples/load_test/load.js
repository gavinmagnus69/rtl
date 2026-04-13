import ws from 'k6/ws';
import { check, sleep } from 'k6';

export const options = {
  scenarios: {
    ws_load: {
      executor: 'constant-vus',
      vus: 200,          // число одновременных соединений
      duration: '2m',    // время теста
    },
  },
};

export default function () {
  const url = 'ws://127.0.0.1:9090'; // поменяй путь при необходимости

  const res = ws.connect(url, {}, function (socket) {
    socket.on('open', () => {
      // шлём сообщения периодически
      socket.setInterval(() => {
        socket.send(JSON.stringify({ type: 'ping', ts: Date.now() }));
      }, 100); // 10 msg/sec на соединение
    });

    socket.on('message', (msg) => {
      // можно добавить проверки протокола
      // console.log(msg);
    });

    socket.on('error', (e) => {
      // ошибки соединения/протокола
    });

    socket.setTimeout(() => {
      socket.close();
    }, 120000);
  });

  check(res, { 'connected': (r) => r && r.status === 101 });
  sleep(1);
}
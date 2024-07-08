package cmd

import (
	"context"
	"errors"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/lesismal/nbio/nbhttp"
	"github.com/lesismal/nbio/nbhttp/websocket"
	"github.com/urfave/cli/v2"
	"go.bug.st/serial"
)

var (
	upgrader   = newUpgrader()
	activeConn *websocket.Conn
	mu         sync.Mutex
)

func newUpgrader() *websocket.Upgrader {
	u := websocket.NewUpgrader()

	u.CheckOrigin = func(r *http.Request) bool {
		return true
	}

	u.OnClose(func(c *websocket.Conn, err error) {
		fmt.Println("OnClose:", c.RemoteAddr().String(), err)

		mu.Lock()
		if activeConn == c {
			activeConn = nil
		}
		mu.Unlock()

	})

	return u
}

func onWebsocket(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)

	if err != nil {
		panic(err)
	}

	fmt.Println("Upgraded:", conn.RemoteAddr().String())
}

func StartWebsocketServer(cCtx *cli.Context) error {
	serverPort := cCtx.Int("port")
	arduinoPort := cCtx.String("board-port")
	arduinoBaudRate := cCtx.Int("baud-rate")
	isDebugMode := cCtx.Bool("debug")

	arduinoMode := &serial.Mode{
		BaudRate: arduinoBaudRate,
	}

	if len(arduinoPort) == 0 {
		fmt.Println("Mohon sebutkan port arduinonya!")

		return nil
	}

	port, arduErr := serial.Open(arduinoPort, arduinoMode)

	if arduErr != nil {
		fmt.Println("Terjadi kesalahan dalam menghubungkan papan arduino, Error:", arduErr)

		return nil
	}

	mux := &http.ServeMux{}
	mux.HandleFunc("/ws", onWebsocket)
	engine := nbhttp.NewEngine(nbhttp.Config{
		Network:                 "tcp",
		Addrs:                   []string{"127.0.0.1:" + strconv.Itoa(serverPort)},
		MaxLoad:                 1000000,
		ReleaseWebsocketPayload: true,
		Handler:                 mux,
	})

	engineErr := engine.Start()

	if engineErr != nil {
		return engineErr
	}

	upgrader.OnOpen(func(c *websocket.Conn) {
		if activeConn != nil {
			fmt.Println("Another client is already connected. Closing the new connection:", c.RemoteAddr().String())

			c.CloseWithError(errors.New("sudah ada client lain yang terhubung"))

			return
		}

		mu.Lock()

		defer mu.Unlock()

		activeConn = c

		fmt.Println("Client terhubung!")

		buff := make([]byte, 100)

		for {
			n, err := port.Read(buff)

			if isDebugMode {
				fmt.Println(n)
				fmt.Println(buff)
			}

			if err != nil {
				c.CloseWithError(errors.Join(errors.New("terjadi kesalahan pada koneksi papan mikrokontroler"), err))

				log.Fatal(err)

				break
			}

			if n == 0 {
				fmt.Println("\nEOF")
				break
			}

			incomingArduinoData := strings.Split(string(buff[:n]), "\r\n")
			santizedNewLine := incomingArduinoData[0]

			if isDebugMode {
				fmt.Println(incomingArduinoData)
				fmt.Println(santizedNewLine)
			}

			if len(santizedNewLine) != 0 {
				// Sebuah fix untuk windows yang baca data yang nanggung dari si
				// arduino yang data seharusnya SORA malah ORA.
				if strings.HasPrefix(santizedNewLine, "SORA-KEYBIND-") || strings.HasPrefix(santizedNewLine, "ORA-KEYBIND-") {
					// Kemudian di normalized disini
					normalizedKeybind := strings.Split(santizedNewLine, "-KEYBIND-")

					if isDebugMode {
						fmt.Println(normalizedKeybind)
					}

					c.WriteMessage(websocket.TextMessage, []byte("SORA-KEYBIND-"+normalizedKeybind[1]))
				}
			}
		}
	})

	interrupt := make(chan os.Signal, 1)
	signal.Notify(interrupt, os.Interrupt)
	<-interrupt

	ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
	defer cancel()
	engine.Shutdown(ctx)

	return nil
}

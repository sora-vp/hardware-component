package main

import (
	"log"
	"os"
	"sw2s/cmd"

	"github.com/urfave/cli/v2"
)

func main() {
	app := &cli.App{
		Name:  "sw2s",
		Usage: "sora websocket server | Sebuah cli app yang menjembatani button module dengan web.",
		Commands: []*cli.Command{
			{
				Name:    "list",
				Aliases: []string{"l", "board", "b"},
				Usage:   "List semua papan arduino yang terhubung dengan perangkat ini.",
				Action:  cmd.ListAllBoard,
			},
			{
				Flags: []cli.Flag{
					&cli.IntFlag{
						Name:    "port",
						Aliases: []string{"p"},
						Value:   3000,
						Usage:   "Pada port berapa server websocket berjalan",
					},

					&cli.StringFlag{
						Name:    "board-port",
						Aliases: []string{"board", "b"},
						Usage:   "Port papan arduino yang terdeteksi oleh perintah list",
					},

					&cli.IntFlag{
						Name:    "baud-rate",
						Aliases: []string{"rate", "r"},
						Value:   9600,
						Usage:   "Pada baud rate berapa papan arduino berjalan",
					},
				},
				Name:    "start",
				Aliases: []string{"s"},
				Usage:   "Membuat server websocket dan menerima data .",
				Action:  cmd.StartWebsocketServer,
			},
		},
	}

	if err := app.Run(os.Args); err != nil {
		log.Fatal(err)
	}
}

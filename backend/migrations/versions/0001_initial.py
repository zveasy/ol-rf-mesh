"""Initial tables

Revision ID: 0001_initial
Revises:
Create Date: 2025-01-05
"""

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = "0001_initial"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_table(
        "nodes",
        sa.Column("node_id", sa.String(), primary_key=True),
        sa.Column("name", sa.String(), nullable=True),
        sa.Column("role", sa.String(), nullable=False),
        sa.Column("hw_serial", sa.String(), nullable=False),
        sa.Column("lat", sa.Float(), nullable=True),
        sa.Column("lon", sa.Float(), nullable=True),
        sa.Column("alt", sa.Float(), nullable=True),
        sa.Column("created_at", sa.DateTime(), nullable=True),
        sa.Column("updated_at", sa.DateTime(), nullable=True),
    )

    op.create_table(
        "rf_events",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("node_id", sa.String(), nullable=False, index=True),
        sa.Column("timestamp", sa.DateTime(), nullable=False, index=True),
        sa.Column("band_id", sa.String(), nullable=False),
        sa.Column("center_freq_hz", sa.Float(), nullable=False),
        sa.Column("bin_width_hz", sa.Float(), nullable=False),
        sa.Column("anomaly_score", sa.Float(), nullable=False),
        sa.Column("features_json", sa.JSON(), nullable=False),
    )

    op.create_table(
        "gps_logs",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("node_id", sa.String(), nullable=False, index=True),
        sa.Column("timestamp", sa.DateTime(), nullable=False, index=True),
        sa.Column("num_sats", sa.Integer(), nullable=False),
        sa.Column("snr_avg", sa.Float(), nullable=False),
        sa.Column("hdop", sa.Float(), nullable=False),
        sa.Column("valid_fix", sa.Boolean(), nullable=False),
        sa.Column("jamming_indicator", sa.Float(), nullable=False),
        sa.Column("spoof_indicator", sa.Float(), nullable=False),
    )

    op.create_table(
        "alerts",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("node_ids_json", sa.JSON(), nullable=False),
        sa.Column("type", sa.String(), nullable=False),
        sa.Column("severity", sa.String(), nullable=False),
        sa.Column("message", sa.String(), nullable=False),
        sa.Column("created_at", sa.DateTime(), nullable=True),
    )

    op.create_table(
        "node_rollups",
        sa.Column("node_id", sa.String(), primary_key=True),
        sa.Column("last_updated", sa.DateTime(), nullable=True),
        sa.Column("rf_events_24h", sa.Integer(), default=0),
        sa.Column("avg_anomaly_24h", sa.Float(), default=0.0),
        sa.Column("gps_jam_events_24h", sa.Integer(), default=0),
    )


def downgrade() -> None:
    op.drop_table("node_rollups")
    op.drop_table("alerts")
    op.drop_table("gps_logs")
    op.drop_table("rf_events")
    op.drop_table("nodes")

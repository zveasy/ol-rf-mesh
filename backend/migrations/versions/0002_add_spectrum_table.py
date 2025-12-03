"""Add spectrum snapshots table

Revision ID: 0002_add_spectrum
Revises: 0001_initial
Create Date: 2025-01-05
"""

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision = "0002_add_spectrum"
down_revision = "0001_initial"
branch_labels = None
depends_on = None


def upgrade() -> None:
    op.create_table(
        "spectrum_snapshots",
        sa.Column("id", sa.Integer(), primary_key=True, autoincrement=True),
        sa.Column("band_id", sa.String(), nullable=False, index=True),
        sa.Column("captured_at", sa.DateTime(), nullable=False, index=True),
        sa.Column("points_json", sa.JSON(), nullable=False),
    )


def downgrade() -> None:
    op.drop_table("spectrum_snapshots")
